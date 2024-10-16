#include "http_request_curl.h"
#include <cstring>
#include "esphome/core/log.h"

namespace esphome {
namespace http_request {

// Initialize static member
bool HttpRequestCurl::curl_initialized_ = false;

// Constructor
HttpRequestCurl::HttpRequestCurl() {
  ESP_LOGV("HttpRequestCurl", "Initializing HttpRequestCurl...");
  if (!initialize_curl()) {
    ESP_LOGE("HttpRequestCurl", "Failed to initialize libcurl");
  }
}

// Destructor
HttpRequestCurl::~HttpRequestCurl() {
  ESP_LOGV("HttpRequestCurl", "Destroying HttpRequestCurl instance");
  // libcurl cleanup is handled globally
}

// Initialize libcurl globally
bool HttpRequestCurl::initialize_curl() {
  if (!curl_initialized_) {
    ESP_LOGV("HttpRequestCurl", "Initializing libcurl globally...");
    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (res != CURLE_OK) {
      ESP_LOGE("HttpRequestCurl", "curl_global_init() failed: %s", curl_easy_strerror(res));
      return false;
    }
    curl_initialized_ = true;
    ESP_LOGV("HttpRequestCurl", "libcurl initialized successfully");
  }
  return true;
}

// Constructor for HttpContainerCurl
HttpContainerCurl::HttpContainerCurl(HttpRequestCurl *parent)
    : parent_(parent), buffer_pos_(0), buffer_len_(0), ended_(false) {
  ESP_LOGV("HttpContainerCurl", "HttpContainerCurl instance created");
}

// Destructor
HttpContainerCurl::~HttpContainerCurl() {
  ESP_LOGV("HttpContainerCurl", "HttpContainerCurl instance destroyed");
}

// Read method implementation
int HttpContainerCurl::read(uint8_t *buf, size_t max_len) {
  ESP_LOGV("HttpContainerCurl", "Reading data from buffer...");
  if (buffer_pos_ >= buffer_len_) {
    if (ended_) {
      ESP_LOGV("HttpContainerCurl", "No more data to read, request has ended");
      return 0;  // No more data
    }
    // Buffer is empty but request not ended
    ESP_LOGW("HttpContainerCurl", "Buffer empty but request not yet ended");
    return 0;
  }

  size_t bytes_available = buffer_len_ - buffer_pos_;
  size_t bytes_to_read = (bytes_available < max_len) ? bytes_available : max_len;
  memcpy(buf, buffer_ + buffer_pos_, bytes_to_read);
  buffer_pos_ += bytes_to_read;
  bytes_read_ += bytes_to_read;
  ESP_LOGI("HttpContainerCurl", "Read %d bytes from buffer", static_cast<int>(bytes_to_read));
  return static_cast<int>(bytes_to_read);
}

// End method implementation
void HttpContainerCurl::end() {
  ESP_LOGI("HttpContainerCurl", "Ending HTTP request");
  ended_ = true;
}

// Write callback for libcurl
size_t HttpContainerCurl::WriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  size_t total_size = size * nmemb;
  ESP_LOGD("HttpContainerCurl", "Writing %zu bytes to buffer", total_size);
  HttpContainerCurl *container = static_cast<HttpContainerCurl *>(userdata);

  size_t bytes_to_copy = (total_size < container->BUFFER_SIZE) ? total_size : container->BUFFER_SIZE;

  // Copy data to buffer
  memcpy(container->buffer_, ptr, bytes_to_copy);
  container->buffer_pos_ = 0;
  container->buffer_len_ = bytes_to_copy;

  ESP_LOGI("HttpContainerCurl", "Stored %zu bytes in buffer", bytes_to_copy);

  return total_size;
}

// Header callback for libcurl (optional, to capture headers)
size_t HttpContainerCurl::HeaderCallback(char *buffer, size_t size, size_t nitems, void *userdata) {
  ESP_LOGD("HttpContainerCurl", "Received header: %.*s", static_cast<int>(size * nitems), buffer);
  return nitems * size;
}

// Start method implementation
std::shared_ptr<HttpContainer> HttpRequestCurl::start(std::string url, std::string method, std::string body,
                                                      std::list<Header> headers) {
  ESP_LOGI("HttpRequestCurl", "Starting HTTP request to URL: %s", url.c_str());

  CURL *curl = curl_easy_init();
  if (!curl) {
    ESP_LOGE("HttpRequestCurl", "curl_easy_init() failed");
    return nullptr;
  }

  // Create a new container
  auto container = std::make_shared<HttpContainerCurl>(this);
  ESP_LOGV("HttpRequestCurl", "Created HttpContainerCurl");

  // Set URL
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  ESP_LOGI("HttpRequestCurl", "Set URL: %s", url.c_str());

  // Set HTTP method and body
  if (method == "POST") {
    ESP_LOGI("HttpRequestCurl", "Using POST method with body");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
  } else if (method == "PUT") {
    ESP_LOGI("HttpRequestCurl", "Using PUT method with body");
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
  } else if (method == "DELETE") {
    ESP_LOGI("HttpRequestCurl", "Using DELETE method");
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
  } else {
    ESP_LOGI("HttpRequestCurl", "Using GET method");
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
  }

  // Set headers
  struct curl_slist *curl_headers = nullptr;
  for (const auto &header : headers) {
    std::string header_str = std::string(header.name) + ": " + std::string(header.value);
    curl_headers = curl_slist_append(curl_headers, header_str.c_str());
    ESP_LOGI("HttpRequestCurl", "Added header: %s", header_str.c_str());
  }
  if (curl_headers) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
  }

  // Set write callback
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, HttpContainerCurl::WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, container.get());
  ESP_LOGD("HttpRequestCurl", "Set write callback");

  // Optional: Set header callback
  // curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HttpContainerCurl::HeaderCallback);
  // curl_easy_setopt(curl, CURLOPT_HEADERDATA, container.get());

  // Set timeout (in seconds; libcurl doesn't support milliseconds directly)
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(this->timeout_) / 1000);
  ESP_LOGI("HttpRequestCurl", "Set timeout to %d seconds", static_cast<long>(this->timeout_) / 1000);

  // Follow redirects
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, this->follow_redirects_ ? 1L : 0L);
  if (this->follow_redirects_) {
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, static_cast<long>(this->redirect_limit_));
    ESP_LOGI("HttpRequestCurl", "Following redirects, max redirects: %d", this->redirect_limit_);
  }

  // Set user agent if provided
  if (this->useragent_) {
    curl_easy_setopt(curl, CURLOPT_USERAGENT, this->useragent_);
    ESP_LOGI("HttpRequestCurl", "Set user agent: %s", this->useragent_);
  }

  // Perform the request
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    ESP_LOGE("HttpRequestCurl", "curl_easy_perform() failed: %s", curl_easy_strerror(res));
    if (curl_headers) {
      curl_slist_free_all(curl_headers);
    }
    curl_easy_cleanup(curl);
    return nullptr;
  }

  // Retrieve response information
  long response_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
  container->set_status_code(response_code);
  ESP_LOGI("HttpRequestCurl", "Received response code: %ld", response_code);

  double total_time = 0.0;
  curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);
  container->set_duration_ms(static_cast<uint32_t>(total_time * 1000));
  ESP_LOGI("HttpRequestCurl", "Request took %.2f seconds", total_time);

  // Optionally retrieve content length
  double cl;
  if (curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &cl) == CURLE_OK) {
    container->set_content_length(static_cast<size_t>(cl));
    ESP_LOGI("HttpRequestCurl", "Content length: %zu", static_cast<size_t>(cl));
  }

  // Cleanup
  if (curl_headers) {
    curl_slist_free_all(curl_headers);
    ESP_LOGD("HttpRequestCurl", "Cleaned up headers");
  }
  curl_easy_cleanup(curl);
  ESP_LOGD("HttpRequestCurl", "Cleaned up curl handle");

  return container;
}

}  // namespace http_request
}  // namespace esphome
