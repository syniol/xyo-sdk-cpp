#include "xyo/client.hpp"

#include <cassert>
#include <functional>
#include <iostream>
#include <memory>

namespace {

class MockTransport final : public xyo::HttpTransport {
 public:
  xyo::HttpResponse response;
  xyo::HttpRequest request;

  xyo::HttpResponse send(const xyo::HttpRequest& value) override {
    request = value;
    return response;
  }
};

bool contains(const std::vector<std::string>& values, const std::string& expected) {
  for (const auto& value : values) if (value == expected) return true;
  return false;
}

void expects_error(const std::function<void()>& operation) {
  try {
    operation();
    assert(false && "expected xyo::Error");
  } catch (const xyo::Error&) {
  }
}

}  // namespace

int main() {
  auto http = std::make_shared<MockTransport>();
  xyo::Client client({"test-key", "https://example.test/", http});

  http->response = {200,
      R"({"merchant":"Syniol Limited","description":"Cloud \u0026 consultancy","categories":["Tech","Finance"],"logo":"base64-data","location":"London","address":null,"cached":true,"confidence":0.98})"};
  auto enrichment = client.enrich_transaction({"COSTA \"PICKUP\"", "GB"});
  assert(enrichment.merchant == "Syniol Limited");
  assert(enrichment.description == "Cloud & consultancy");
  assert(enrichment.categories.size() == 2);
  assert(enrichment.location == "London");
  assert(!enrichment.address);
  assert(http->request.method == "POST");
  assert(http->request.url == "https://example.test/v1/ai/finance/enrichment/transaction");
  assert(http->request.body == R"({"content":"COSTA \"PICKUP\"","countryCode":"GB"})");
  assert(contains(http->request.headers, "Authorization: Bearer test-key"));
  assert(contains(http->request.headers, "Content-Type: application/json"));

  http->response = {200, R"({"id":"work-123","link":"https://example.test/results.tar.gz"})"};
  auto collection = client.enrich_transaction_collection({{"Costa", "GB"}, {"Starbucks", "US"}});
  assert(collection.id == "work-123");
  assert(http->request.url == "https://example.test/v1/ai/finance/enrichment/transactions");
  assert(http->request.body == R"([{"content":"Costa","countryCode":"GB"},{"content":"Starbucks","countryCode":"US"}])");

  http->response = {200, R"({"status":"READY"})"};
  assert(client.enrich_transaction_collection_status("work-123") ==
         xyo::EnrichmentCollectionStatus::ready);
  assert(http->request.url ==
         "https://example.test/v1/ai/finance/enrichment/transactions/status/work-123");
  assert(xyo::to_string(xyo::EnrichmentCollectionStatus::pending) == "PENDING");

  http->response = {400, R"({"error":"bad request"})"};
  expects_error([&] { client.enrich_transaction({"bad", "GB"}); });

  http->response = {200, R"({"status":"UNKNOWN"})"};
  expects_error([&] { client.enrich_transaction_collection_status("work-123"); });
  expects_error([] { xyo::Client({"", "https://example.test", nullptr}); });

  std::cout << "All XYO C++ SDK tests passed\n";
}
