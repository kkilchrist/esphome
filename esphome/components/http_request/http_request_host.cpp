// http_request_host.cpp

#include "http_request_host.h"
#include "esphome/core/log.h"

#ifdef USE_HOST
#include <chrono>
#include <curl/curl.h>
#include <memory>
#include <string>
#include <list>

namespace esphome {
namespace http_request {

static const char *TAG = "http_request_host";

// Utility function to convert milliseconds to seconds (for libcurl timeout)
static double ms_to_seconds(int ms) { return static_cast<double>(ms) / 1000.0; }

// Write callback for libcurl to capture response data
size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
  size_t total_size = size * nmemb;
  std::string *response = static_cast<std::string *>(userp);
  response->append(static_cast<char *>(contents), total_size);
  return total_size;
}

// Implementation of HttpRequestHostPlatform

HttpRequestHostPlatform::HttpRequestHostPlatform() : HttpContainer() {
  ESP_LOGD(TAG, "Initializing HttpRequestHostPlatform");

  // Initialize CURL globally if not already done
  curl_global_init(CURL_GLOBAL_ALL);

  // Initialize CURL handle
  curl_handle_ = curl_easy_init();
  if (!curl_handle_) {
    ESP_LOGE(TAG, "Failed to initialize CURL handle");
  } else {
    ESP_LOGD(TAG, "CURL handle initialized successfully");
  }
}

HttpRequestHostPlatform::~HttpRequestHostPlatform() {
  ESP_LOGD(TAG, "Destroying HttpRequestHostPlatform");

  if (curl_handle_) {
    curl_easy_cleanup(curl_handle_);
    curl_handle_ = nullptr;
    ESP_LOGD(TAG, "CURL handle cleaned up");
  }

  // Cleanup CURL globally
  curl_global_cleanup();
}

void HttpRequestHostPlatform::set_timeout(int timeout_ms) {
  this->timeout_ms_ = timeout_ms;
  if (curl_handle_) {
    curl_easy_setopt(curl_handle_, CURLOPT_TIMEOUT, ms_to_seconds(this->timeout_ms_));
    ESP_LOGD(TAG, "Set timeout to %d ms", this->timeout_ms_);
  }
}

void HttpRequestHostPlatform::set_useragent(const std::string &useragent) {
  this->useragent_ = useragent;
  if (curl_handle_) {
    curl_easy_setopt(curl_handle_, CURLOPT_USERAGENT, this->useragent_.c_str());
    ESP_LOGD(TAG, "Set User-Agent to '%s'", this->useragent_.c_str());
  }
}

void HttpRequestHostPlatform::set_follow_redirects(bool follow) {
  this->follow_redirects_ = follow;
  if (curl_handle_) {
    curl_easy_setopt(curl_handle_, CURLOPT_FOLLOWLOCATION, this->follow_redirects_ ? 1L : 0L);
    ESP_LOGD(TAG, "Set Follow Redirects to %s", this->follow_redirects_ ? "Yes" : "No");
  }
}

void HttpRequestHostPlatform::set_redirect_limit(int limit) {
  this->redirect_limit_ = limit;
  if (curl_handle_) {
    curl_easy_setopt(curl_handle_, CURLOPT_MAXREDIRS, this->redirect_limit_);
    ESP_LOGD(TAG, "Set Redirect Limit to %d", this->redirect_limit_);
  }
}

void HttpRequestHostPlatform::set_response(const std::string &response) {
  this->response_ = response;
  this->content_length = response.size();
}

int HttpRequestHostPlatform::read(uint8_t *buf, size_t max_len) {
  size_t remaining = this->response_.size() - this->bytes_read_;
  size_t to_read = std::min(max_len, remaining);
  if (to_read > 0) {
    memcpy(buf, this->response_.data() + this->bytes_read_, to_read);
    this->bytes_read_ += to_read;
  }
  ESP_LOGD(TAG, "Read %zu bytes, %zu bytes remaining", to_read, remaining - to_read);
  return static_cast<int>(to_read);
}

void HttpRequestHostPlatform::end() {
  ESP_LOGD(TAG, "Ending HTTP request");
  // Any cleanup after reading can be done here
}


}  // namespace http_request
}  // namespace esphome

#endif  // USE_HOST
