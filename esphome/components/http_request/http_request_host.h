#pragma once

#include "http_request.h"
#include <curl/curl.h>
#include <memory>
#include <string>
#include <vector>

namespace esphome {
namespace http_request {

class HttpRequestHostPlatform : public HttpContainer {
 public:
  HttpRequestHostPlatform();
  ~HttpRequestHostPlatform();

  void set_timeout(int timeout_ms) ;
  void set_useragent(const std::string &useragent) ;
  void set_follow_redirects(bool follow) ;
  void set_redirect_limit(int limit) ;
  void set_response(const std::string &response) ;
  int read(uint8_t *buf, size_t max_len) ;
  void end() ;

  friend class HttpRequestHost;

 protected:
  CURL *curl_handle_;
  std::string response_;
  size_t bytes_read_{0};
  std::string useragent_;  // Add this line
  int timeout_ms_{5000};
  int redirect_limit_{5};
  bool follow_redirects_{true};

};


// Component class for initiating HTTP requests on Host Platform
class HttpRequestHost : public HttpRequestComponent {
 public:
  HttpRequestHost() {
    // Initialization if needed
  }

  ~HttpRequestHost() {
    // Cleanup if needed
  }

  // Implementation of virtual method to start HTTP request
  std::shared_ptr<HttpContainer> start(std::string url, std::string method, std::string body,
                                       std::list<Header> headers) override {
    // Create a new HttpRequestHostPlatform container for this request
    auto container = std::make_shared<HttpRequestHostPlatform>();

    // Configure CURL for this request
    curl_easy_setopt(container->curl_handle_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(container->curl_handle_, CURLOPT_CUSTOMREQUEST, method.c_str());
    
    // Set headers if available
    struct curl_slist *header_list = nullptr;
    for (const auto &header : headers) {
      std::string header_string = std::string(header.name) + ": " + std::string(header.value);
      header_list = curl_slist_append(header_list, header_string.c_str());
    }
    if (header_list) {
      curl_easy_setopt(container->curl_handle_, CURLOPT_HTTPHEADER, header_list);
    }

    // Set the body for POST/PUT requests
    if (method == "POST" || method == "PUT") {
      curl_easy_setopt(container->curl_handle_, CURLOPT_POSTFIELDS, body.c_str());
    }

    // Execute the request and capture the response
    CURLcode res = curl_easy_perform(container->curl_handle_);
    if (res == CURLE_OK) {
      char *response_data;
      long response_code;
      curl_easy_getinfo(container->curl_handle_, CURLINFO_RESPONSE_CODE, &response_code);
      curl_easy_getinfo(container->curl_handle_, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &container->content_length);

      // Assuming the response can be captured in memory
      curl_easy_setopt(container->curl_handle_, CURLOPT_WRITEFUNCTION, WriteCallback);
      curl_easy_setopt(container->curl_handle_, CURLOPT_WRITEDATA, &response_data);
      container->set_response(response_data);
    }

    return container;
  }

 protected:
  static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string *)userp)->append((char *)contents, size * nmemb);
    return size * nmemb;
  }

  CURL *curl_handle_;
};

}  // namespace http_request
}  // namespace esphome
