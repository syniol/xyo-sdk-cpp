#include "xyo/client.hpp"

#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

inline void test_check(bool condition, const char* condition_str, const char* file, int line) {
  if (!condition) {
    std::cerr << "Assertion failed: (" << condition_str << ") at " << file << ":" << line << "\n";
    std::exit(1);
  }
}

#define TEST_ASSERT(cond) test_check(!!(cond), #cond, __FILE__, __LINE__)

class MockTransport final : public xyo::HttpTransport {
 public:
  xyo::HttpResponse response;
  xyo::HttpRequest request;

  xyo::HttpResponse send(const xyo::HttpRequest& value) override {
    request = value;
    return response;
  }
};

bool contains(const std::vector<std::pair<std::string, std::string>>& headers, const std::string& key, const std::string& value) {
  for (const auto& h : headers) {
    if (h.first == key && h.second == value) return true;
  }
  return false;
}

void expects_error(xyo::ErrorCategory expected_category, const std::string& expected_message_substring, const std::function<void()>& operation) {
  try {
    operation();
    std::cerr << "Expected error with category and message containing '" << expected_message_substring << "' but no exception was thrown.\n";
    std::exit(1);
  } catch (const xyo::Error& e) {
    TEST_ASSERT(e.category() == expected_category);
    TEST_ASSERT(e.what() != nullptr);
    std::string msg(e.what());
    TEST_ASSERT(msg.find(expected_message_substring) != std::string::npos);
  }
}

}  // namespace

int main() {
  auto http = std::make_shared<MockTransport>();

  // 1. Basic success cases
  xyo::Client client({"test-key", "https://example.test/", http});

  http->response = {200,
      R"({"merchant":"Syniol Limited","description":"Cloud \u0026 consultancy","categories":["Tech","Finance"],"logo":"base64-data","location":"London","address":null,"cached":true,"confidence":0.98})"};
  auto enrichment = client.enrich_transaction({"COSTA \"PICKUP\"", "GB"});
  TEST_ASSERT(enrichment.merchant == "Syniol Limited");
  TEST_ASSERT(enrichment.description == "Cloud & consultancy");
  TEST_ASSERT(enrichment.categories.size() == 2);
  TEST_ASSERT(enrichment.location == "London");
  TEST_ASSERT(!enrichment.address);
  TEST_ASSERT(http->request.method == "POST");
  TEST_ASSERT(http->request.url == "https://example.test/v1/ai/finance/enrichment/transaction");
  TEST_ASSERT(http->request.body == R"({"content":"COSTA \"PICKUP\"","countryCode":"GB"})");
  TEST_ASSERT(contains(http->request.headers, "Authorization", "Bearer test-key"));
  TEST_ASSERT(contains(http->request.headers, "Content-Type", "application/json"));

  http->response = {200, R"({"id":"work-123","link":"https://example.test/results.tar.gz"})"};
  auto collection = client.enrich_transaction_collection({{"Costa", "GB"}, {"Starbucks", "US"}});
  TEST_ASSERT(collection.id == "work-123");
  TEST_ASSERT(http->request.url == "https://example.test/v1/ai/finance/enrichment/transactions");
  TEST_ASSERT(http->request.body == R"([{"content":"Costa","countryCode":"GB"},{"content":"Starbucks","countryCode":"US"}])");

  http->response = {200, R"({"status":"READY"})"};
  TEST_ASSERT(client.enrich_transaction_collection_status("work-123") ==
         xyo::EnrichmentCollectionStatus::ready);
  TEST_ASSERT(http->request.url ==
         "https://example.test/v1/ai/finance/enrichment/transactions/status/work-123");
  TEST_ASSERT(xyo::to_string(xyo::EnrichmentCollectionStatus::pending) == "PENDING");

  // 2. HTTP Error handling and category checks
  http->response = {400, R"({"error":"bad request"})"};
  expects_error(xyo::ErrorCategory::http, "status code 400", [&] {
    client.enrich_transaction({"bad", "GB"});
  });

  // Verify that HTTP status code is carried in Error
  try {
    client.enrich_transaction({"bad", "GB"});
    TEST_ASSERT(false);
  } catch (const xyo::Error& e) {
    TEST_ASSERT(e.category() == xyo::ErrorCategory::http);
    TEST_ASSERT(e.http_status_code() == 400);
  }

  // 3. Validation errors
  http->response = {200, R"({"status":"UNKNOWN"})"};
  expects_error(xyo::ErrorCategory::validation, "unknown enrichment collection status: UNKNOWN", [&] {
    client.enrich_transaction_collection_status("work-123");
  });

  expects_error(xyo::ErrorCategory::validation, "api_key must not be empty", [] {
    xyo::Client({"", "https://example.test", nullptr});
  });

  // 4. Insecure HTTP validation check
  expects_error(xyo::ErrorCategory::validation, "api_base_url must use HTTPS unless allow_insecure_http is enabled", [] {
    xyo::Client({"test-key", "http://insecure-api.xyo.financial", nullptr});
  });

  // Allowed if explicitly requested
  {
    xyo::ClientConfig insecure_config("test-key", "http://insecure-api.xyo.financial", nullptr);
    insecure_config.allow_insecure_http = true;
    xyo::Client insecure_client(insecure_config);
  }

  // 5. JSON Parsing constraints: duplicate key detection
  http->response = {200, R"({"merchant":"Costa","merchant":"Duplicate"})"};
  expects_error(xyo::ErrorCategory::parsing, "duplicate key in JSON object", [&] {
    client.enrich_transaction({"Costa", "GB"});
  });

  // 6. JSON Parsing constraints: max depth detection
  {
    xyo::ClientConfig depth_config{"test-key", "https://example.test", http};
    depth_config.max_json_depth = 3;
    xyo::Client depth_client(depth_config);

    // Depth = 4 (object -> array -> array -> array -> scalar)
    http->response = {200, R"({"merchant":[[[1]]],"description":"test","logo":"test","categories":[]})"};
    expects_error(xyo::ErrorCategory::parsing, "exceeded maximum JSON depth limit", [&] {
      depth_client.enrich_transaction({"Costa", "GB"});
    });
  }

  // 7. JSON Parsing constraints: max node detection
  {
    xyo::ClientConfig node_config{"test-key", "https://example.test", http};
    node_config.max_json_nodes = 5;
    xyo::Client node_client(node_config);

    http->response = {200, R"({"merchant":"Costa","description":"Coffee","logo":"url","categories":["a","b"]})"};
    expects_error(xyo::ErrorCategory::parsing, "exceeded maximum JSON node limit", [&] {
      node_client.enrich_transaction({"Costa", "GB"});
    });
  }

  // 8. Collection size limit validation
  {
    xyo::ClientConfig coll_config{"test-key", "https://example.test", http};
    coll_config.max_collection_size = 2;
    xyo::Client coll_client(coll_config);

    expects_error(xyo::ErrorCategory::validation, "collection size exceeds max_collection_size limit", [&] {
      coll_client.enrich_transaction_collection({{"a", "GB"}, {"b", "US"}, {"c", "CA"}});
    });
  }

  // 9. ID path traversal prevention validation
  expects_error(xyo::ErrorCategory::validation, "invalid transaction collection ID format", [&] {
    client.enrich_transaction_collection_status("../malicious-id");
  });
  expects_error(xyo::ErrorCategory::validation, "invalid transaction collection ID format", [&] {
    client.enrich_transaction_collection_status("id?param=value");
  });

  std::cout << "All XYO C++ SDK tests passed\n";
  return 0;
}
