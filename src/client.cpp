#include "xyo/client.hpp"

#include <curl/curl.h>

#include <cctype>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <utility>

namespace xyo {
namespace {

struct Json {
  enum class Kind { null_value, string, scalar, array, object } kind = Kind::null_value;
  std::string text;
  std::vector<Json> array;
  std::map<std::string, Json> object;
};

class JsonParser {
 public:
  explicit JsonParser(const std::string& input) : input_(input) {}

  Json parse() {
    Json result = value();
    whitespace();
    if (position_ != input_.size()) fail("trailing content");
    return result;
  }

 private:
  const std::string& input_;
  std::size_t position_ = 0;

  [[noreturn]] void fail(const std::string& message) const {
    throw Error("invalid JSON response at byte " + std::to_string(position_) +
                ": " + message);
  }

  void whitespace() {
    while (position_ < input_.size() &&
           std::isspace(static_cast<unsigned char>(input_[position_]))) ++position_;
  }

  bool consume(char expected) {
    whitespace();
    if (position_ < input_.size() && input_[position_] == expected) {
      ++position_;
      return true;
    }
    return false;
  }

  Json value() {
    whitespace();
    if (position_ >= input_.size()) fail("expected a value");
    if (input_[position_] == '"') {
      Json result;
      result.kind = Json::Kind::string;
      result.text = string();
      return result;
    }
    if (input_[position_] == '{') return object();
    if (input_[position_] == '[') return array();
    if (input_.compare(position_, 4, "null") == 0) {
      position_ += 4;
      return {};
    }
    if (input_.compare(position_, 4, "true") == 0) return scalar(4);
    if (input_.compare(position_, 5, "false") == 0) return scalar(5);
    if (input_[position_] == '-' ||
        std::isdigit(static_cast<unsigned char>(input_[position_]))) return number();
    fail("unsupported value");
  }

  Json scalar(std::size_t length) {
    Json result;
    result.kind = Json::Kind::scalar;
    result.text = input_.substr(position_, length);
    position_ += length;
    return result;
  }

  Json number() {
    std::size_t start = position_;
    if (input_[position_] == '-') ++position_;
    if (position_ >= input_.size()) fail("invalid number");
    if (input_[position_] == '0') ++position_;
    else {
      if (!std::isdigit(static_cast<unsigned char>(input_[position_]))) fail("invalid number");
      while (position_ < input_.size() &&
             std::isdigit(static_cast<unsigned char>(input_[position_]))) ++position_;
    }
    if (position_ < input_.size() && input_[position_] == '.') {
      ++position_;
      if (position_ >= input_.size() ||
          !std::isdigit(static_cast<unsigned char>(input_[position_]))) fail("invalid number");
      while (position_ < input_.size() &&
             std::isdigit(static_cast<unsigned char>(input_[position_]))) ++position_;
    }
    if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
      ++position_;
      if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-'))
        ++position_;
      if (position_ >= input_.size() ||
          !std::isdigit(static_cast<unsigned char>(input_[position_]))) fail("invalid number");
      while (position_ < input_.size() &&
             std::isdigit(static_cast<unsigned char>(input_[position_]))) ++position_;
    }
    return scalar_from_range(start);
  }

  Json scalar_from_range(std::size_t start) {
    Json result;
    result.kind = Json::Kind::scalar;
    result.text = input_.substr(start, position_ - start);
    return result;
  }

  std::string string() {
    if (input_[position_++] != '"') fail("expected string");
    std::string result;
    while (position_ < input_.size()) {
      char c = input_[position_++];
      if (c == '"') return result;
      if (c != '\\') {
        result += c;
        continue;
      }
      if (position_ >= input_.size()) fail("unfinished escape");
      switch (input_[position_++]) {
        case '"': result += '"'; break;
        case '\\': result += '\\'; break;
        case '/': result += '/'; break;
        case 'b': result += '\b'; break;
        case 'f': result += '\f'; break;
        case 'n': result += '\n'; break;
        case 'r': result += '\r'; break;
        case 't': result += '\t'; break;
        case 'u': {
          unsigned int codepoint = hex_quad();
          if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
            if (position_ + 2 > input_.size() || input_[position_] != '\\' ||
                input_[position_ + 1] != 'u') fail("invalid Unicode surrogate pair");
            position_ += 2;
            unsigned int low = hex_quad();
            if (low < 0xDC00 || low > 0xDFFF) fail("invalid Unicode surrogate pair");
            codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
          } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
            fail("unexpected low Unicode surrogate");
          }
          append_utf8(result, codepoint);
          break;
        }
        default: fail("unsupported escape");
      }
    }
    fail("unterminated string");
  }

  unsigned int hex_quad() {
    if (position_ + 4 > input_.size()) fail("unfinished Unicode escape");
    unsigned int value = 0;
    for (int i = 0; i < 4; ++i) {
      char c = input_[position_++];
      value <<= 4;
      if (c >= '0' && c <= '9') value += static_cast<unsigned int>(c - '0');
      else if (c >= 'a' && c <= 'f') value += static_cast<unsigned int>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') value += static_cast<unsigned int>(c - 'A' + 10);
      else fail("invalid Unicode escape");
    }
    return value;
  }

  static void append_utf8(std::string& output, unsigned int codepoint) {
    if (codepoint <= 0x7F) output += static_cast<char>(codepoint);
    else if (codepoint <= 0x7FF) {
      output += static_cast<char>(0xC0 | (codepoint >> 6));
      output += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
      output += static_cast<char>(0xE0 | (codepoint >> 12));
      output += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
      output += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else {
      output += static_cast<char>(0xF0 | (codepoint >> 18));
      output += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
      output += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
      output += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
  }

  Json array() {
    ++position_;
    Json result;
    result.kind = Json::Kind::array;
    if (consume(']')) return result;
    do { result.array.push_back(value()); } while (consume(','));
    if (!consume(']')) fail("expected ']'");
    return result;
  }

  Json object() {
    ++position_;
    Json result;
    result.kind = Json::Kind::object;
    if (consume('}')) return result;
    do {
      whitespace();
      if (position_ >= input_.size() || input_[position_] != '"') fail("expected key");
      std::string key = string();
      if (!consume(':')) fail("expected ':'");
      result.object.emplace(std::move(key), value());
    } while (consume(','));
    if (!consume('}')) fail("expected '}'");
    return result;
  }
};

const Json& required(const Json& object, const std::string& name, Json::Kind kind) {
  if (object.kind != Json::Kind::object) throw Error("JSON response must be an object");
  auto item = object.object.find(name);
  if (item == object.object.end() || item->second.kind != kind)
    throw Error("JSON response field '" + name + "' is missing or has the wrong type");
  return item->second;
}

std::optional<std::string> optional_string(const Json& object, const std::string& name) {
  auto item = object.object.find(name);
  if (item == object.object.end() || item->second.kind == Json::Kind::null_value)
    return std::nullopt;
  if (item->second.kind != Json::Kind::string)
    throw Error("JSON response field '" + name + "' has the wrong type");
  return item->second.text;
}

std::string json_string(const std::string& input) {
  std::ostringstream output;
  output << '"';
  for (unsigned char c : input) {
    switch (c) {
      case '"': output << "\\\""; break;
      case '\\': output << "\\\\"; break;
      case '\b': output << "\\b"; break;
      case '\f': output << "\\f"; break;
      case '\n': output << "\\n"; break;
      case '\r': output << "\\r"; break;
      case '\t': output << "\\t"; break;
      default:
        if (c < 0x20) output << "\\u" << std::hex << std::setw(4)
                             << std::setfill('0') << static_cast<int>(c);
        else output << c;
    }
  }
  output << '"';
  return output.str();
}

std::string request_json(const EnrichmentRequest& request) {
  return "{\"content\":" + json_string(request.content) +
         ",\"countryCode\":" + json_string(request.country_code) + "}";
}

size_t write_body(char* data, size_t size, size_t count, void* target) {
  static_cast<std::string*>(target)->append(data, size * count);
  return size * count;
}

class CurlTransport final : public HttpTransport {
 public:
  HttpResponse send(const HttpRequest& request) override {
    static std::once_flag init;
    std::call_once(init, [] {
      if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
        throw Error("failed to initialize libcurl");
    });
    CURL* curl = curl_easy_init();
    if (!curl) throw Error("failed to create libcurl request");
    struct curl_slist* headers = nullptr;
    for (const auto& header : request.headers) headers = curl_slist_append(headers, header.c_str());
    HttpResponse response;
    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "xyo-sdk-cpp/1.0");
    CURLcode code = curl_easy_perform(curl);
    if (code == CURLE_OK) curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
    std::string error = curl_easy_strerror(code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (code != CURLE_OK) throw Error("HTTP request failed: " + error);
    return response;
  }
};

}  // namespace

std::string to_string(EnrichmentCollectionStatus status) {
  switch (status) {
    case EnrichmentCollectionStatus::ready: return "READY";
    case EnrichmentCollectionStatus::failed: return "FAILED";
    case EnrichmentCollectionStatus::pending: return "PENDING";
  }
  throw Error("unknown enrichment collection status");
}

Client::Client(ClientConfig config) : config_(std::move(config)) {
  if (config_.api_key.empty()) throw Error("api_key must not be empty");
  if (config_.api_base_url.empty()) throw Error("api_base_url must not be empty");
  while (config_.api_base_url.size() > 1 && config_.api_base_url.back() == '/')
    config_.api_base_url.pop_back();
  if (!config_.http_transport) config_.http_transport = std::make_shared<CurlTransport>();
}

HttpResponse Client::post(const std::string& path, const std::string& body) const {
  HttpRequest request{"POST", config_.api_base_url + path,
                      {"Content-Type: application/json", "Accept: application/json",
                       "Authorization: Bearer " + config_.api_key}, body};
  HttpResponse response = config_.http_transport->send(request);
  if (response.status_code != 200)
    throw Error("XYO API returned status code " + std::to_string(response.status_code));
  return response;
}

EnrichmentResponse Client::enrich_transaction(const EnrichmentRequest& request) const {
  Json root = JsonParser(post("/v1/ai/finance/enrichment/transaction", request_json(request)).body).parse();
  EnrichmentResponse response;
  response.merchant = required(root, "merchant", Json::Kind::string).text;
  response.description = required(root, "description", Json::Kind::string).text;
  response.logo = required(root, "logo", Json::Kind::string).text;
  for (const auto& category : required(root, "categories", Json::Kind::array).array) {
    if (category.kind != Json::Kind::string) throw Error("categories must contain strings");
    response.categories.push_back(category.text);
  }
  response.location = optional_string(root, "location");
  response.address = optional_string(root, "address");
  return response;
}

EnrichTransactionCollectionResponse Client::enrich_transaction_collection(
    const std::vector<EnrichmentRequest>& requests) const {
  std::string body = "[";
  for (std::size_t i = 0; i < requests.size(); ++i) {
    if (i) body += ',';
    body += request_json(requests[i]);
  }
  body += ']';
  Json root = JsonParser(post("/v1/ai/finance/enrichment/transactions", body).body).parse();
  return {required(root, "id", Json::Kind::string).text,
          required(root, "link", Json::Kind::string).text};
}

EnrichmentCollectionStatus Client::enrich_transaction_collection_status(
    const std::string& id) const {
  if (id.empty()) throw Error("id must not be empty");
  Json root = JsonParser(post("/v1/ai/finance/enrichment/transactions/status/" + id, "").body).parse();
  const std::string& status = required(root, "status", Json::Kind::string).text;
  if (status == "READY") return EnrichmentCollectionStatus::ready;
  if (status == "FAILED") return EnrichmentCollectionStatus::failed;
  if (status == "PENDING") return EnrichmentCollectionStatus::pending;
  throw Error("unknown enrichment collection status: " + status);
}

}  // namespace xyo
