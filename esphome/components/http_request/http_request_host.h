#pragma once

#include "http_request.h"
#include <curl/curl.h>
#include <memory>
#include <string>
#include <vector>

namespace esphome {
namespace http_request {

static const char *TAG = "http_request.host";

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
  std::string useragent_;
  int timeout_ms_{5000};
  int redirect_limit_{5};
  bool follow_redirects_{true};

};


// Component class for initiating HTTP requests on Host Platform
class HttpRequestHost : public HttpRequestComponent {
 public:
  HttpRequestHost() {
    
  }

  ~HttpRequestHost() {
    
  }

std::shared_ptr<HttpContainer> start(std::string url, std::string method, std::string body,
                                     std::list<Header> headers) override {
  // Create a new HttpRequestHostPlatform container for this request
  auto container = std::make_shared<HttpRequestHostPlatform>();

  // Initialize CURL for this request
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
  
  // Configure to write response to the buffer instead of stdout
  curl_easy_setopt(container->curl_handle_, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(container->curl_handle_, CURLOPT_WRITEDATA, &container->response_);
  
  // Start measuring request time
  auto start_time = std::chrono::high_resolution_clock::now();

  // Execute the request
  CURLcode res = curl_easy_perform(container->curl_handle_);
  if (res != CURLE_OK) {
    // Handle the error case
    ESP_LOGE(TAG, "CURL request failed: %s", curl_easy_strerror(res));
  } else {
    // Get status code
    long response_code;
    curl_easy_getinfo(container->curl_handle_, CURLINFO_RESPONSE_CODE, &response_code);
    container->status_code = static_cast<int>(response_code);

    // Get content length
    double content_length = 0;
    curl_easy_getinfo(container->curl_handle_, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &content_length);
    container->content_length = static_cast<size_t>(content_length);
    

    // Measure the duration of the request
    auto end_time = std::chrono::high_resolution_clock::now();
    container->duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    ESP_LOGD(TAG, "Request successful. Response code: %ld, Content-Length: %zu, Duration: %u ms",
             response_code, container->content_length, container->duration_ms);
  }

  // Clean up headers
  if (header_list) {
    curl_slist_free_all(header_list);
  }

  return container;
}

 protected:
  static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t total_size = size * nmemb;
  std::string *response = static_cast<std::string *>(userp);
  response->append(static_cast<char *>(contents), total_size);
  return total_size;
}


  CURL *curl_handle_;
};

}  // namespace http_request
}  // namespace esphome
