// Copyright (c) 2025 Syniol Limited
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef XYO_SDK_EXPORTS
    #define XYO_SDK_API __declspec(dllexport)
  #elif defined(XYO_SDK_SHARED)
    #define XYO_SDK_API __declspec(dllimport)
  #else
    #define XYO_SDK_API
  #endif
#else
  #if __GNUC__ >= 4
    #define XYO_SDK_API __attribute__((visibility("default")))
  #else
    #define XYO_SDK_API
  #endif
#endif

namespace xyo {

struct XYO_SDK_API EnrichmentRequest {
  std::string content;
  std::string country_code;
};

struct XYO_SDK_API EnrichmentResponse {
  std::string merchant;
  std::string description;
  std::vector<std::string> categories;
  std::string logo;
  std::optional<std::string> location;
  std::optional<std::string> address;
};

struct XYO_SDK_API EnrichTransactionCollectionResponse {
  std::string id;
  std::string link;
};

enum class EnrichmentCollectionStatus { ready, failed, pending };

XYO_SDK_API std::string to_string(EnrichmentCollectionStatus status);

struct XYO_SDK_API HttpRequest {
  std::string method;
  std::string url;
  std::vector<std::string> headers;
  std::string body;
};

struct XYO_SDK_API HttpResponse {
  long status_code = 0;
  std::string body;
};

class XYO_SDK_API HttpTransport {
 public:
  virtual ~HttpTransport() = default;
  virtual HttpResponse send(const HttpRequest& request) = 0;
};

struct XYO_SDK_API ClientConfig {
  std::string api_key;
  std::string api_base_url = "https://api.xyo.financial";
  std::shared_ptr<HttpTransport> http_transport;

  // These limits are applied by the built-in libcurl transport. Response and
  // JSON limits are also enforced when a custom transport is supplied.
  long connect_timeout_ms = 5'000;
  long request_timeout_ms = 30'000;
  long low_speed_timeout_seconds = 15;
  long low_speed_limit_bytes_per_second = 100;
  std::size_t max_response_bytes = 1024 * 1024;
  std::size_t max_json_depth = 64;
  std::size_t max_json_nodes = 100'000;
  std::size_t max_collection_size = 1'000;

  // Intended only for local development with the built-in transport. HTTPS
  // remains mandatory unless this is explicitly enabled.
  bool allow_insecure_http = false;
};

enum class ErrorCategory { validation, transport, http, parsing, protocol };

class XYO_SDK_API Error : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;

  Error(ErrorCategory category, std::string message, long http_status_code = 0,
        int transport_code = 0);

  ErrorCategory category() const noexcept { return category_; }
  long http_status_code() const noexcept { return http_status_code_; }
  int transport_code() const noexcept { return transport_code_; }

 private:
  ErrorCategory category_ = ErrorCategory::protocol;
  long http_status_code_ = 0;
  int transport_code_ = 0;
};

class XYO_SDK_API Client {
 public:
  explicit Client(ClientConfig config);

  EnrichmentResponse enrich_transaction(const EnrichmentRequest& request) const;
  EnrichTransactionCollectionResponse enrich_transaction_collection(
      const std::vector<EnrichmentRequest>& requests) const;
  EnrichmentCollectionStatus enrich_transaction_collection_status(
      const std::string& id) const;

 private:
  ClientConfig config_;
  HttpResponse post(const std::string& path, const std::string& body) const;
};

}  // namespace xyo
