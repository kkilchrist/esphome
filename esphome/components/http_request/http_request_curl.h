#pragma once

#include "http_request.h"
#include <curl/curl.h>
#include <memory>
#include <string>
#include <list>
#include <functional>

namespace esphome {
namespace http_request {

// Forward declaration
class HttpRequestCurl;

// HttpContainerCurl inherits from HttpContainer
class HttpContainerCurl : public HttpContainer {
 public:
  HttpContainerCurl(HttpRequestCurl *parent);
  ~HttpContainerCurl() override;

  // Overrides from HttpContainer
  int read(uint8_t *buf, size_t max_len) override;
  void end() override;

  // Methods to handle data from libcurl
  static size_t WriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata);
  static size_t HeaderCallback(char *buffer, size_t size, size_t nitems, void *userdata);

  // Setters for response metadata
  void set_status_code(long code) { this->status_code = static_cast<int>(code); }
  void set_content_length(long length) { this->content_length = static_cast<size_t>(length); }
  void set_duration_ms(uint32_t duration) { this->duration_ms = duration; }

 protected:
  HttpRequestCurl *parent_;
  
  // Fixed-size buffer to store response data
  static constexpr size_t BUFFER_SIZE = 1024;
  uint8_t buffer_[BUFFER_SIZE];
  size_t buffer_pos_;
  size_t buffer_len_;

  // Flags to indicate if the request has ended
  bool ended_;
};

// HttpRequestCurl inherits from HttpRequestComponent
class HttpRequestCurl : public HttpRequestComponent {
 public:
  HttpRequestCurl();
  ~HttpRequestCurl() ;

  // Overrides from HttpRequestComponent
  std::shared_ptr<HttpContainer> start(std::string url, std::string method, std::string body,
                                       std::list<Header> headers) override;

 private:
  // Initialize libcurl globally
  static bool curl_initialized_;
  static bool initialize_curl();

  // Disable copy constructor and assignment
  HttpRequestCurl(const HttpRequestCurl &) = delete;
  HttpRequestCurl &operator=(const HttpRequestCurl &) = delete;
};

}  // namespace http_request
}  // namespace esphome
