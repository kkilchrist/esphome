#pragma once

#include "image_decoder.h"
#include "esphome/core/log.h"
#include "webp/decode.h"
#include "online_image.h"

namespace esphome {
namespace online_image {

static const char *const TAG = "online_image.webp";

/**
 * @brief Image decoder specialization for WebP images using libwebp with conditional decoding based on image size.
 */
class WebPDecoder : public ImageDecoder {
 public:
  WebPDecoder(OnlineImage *parent)
      : ImageDecoder(parent),
        parent_(parent),
        decoder_ready_(false),
        webp_decoder_(nullptr),
        size_set_(false) {}

  ~WebPDecoder() override {
    if (webp_decoder_ != nullptr) {
      WebPIDelete(webp_decoder_);
    }
  }

  void prepare(uint32_t download_size) override {
    ESP_LOGD(TAG, "WebPDecoder::prepare called with total_size: %u", download_size);
    this->download_size_ = download_size;
    this->decoded_bytes_ = 0;
    this->size_set_ = false;
    this->decoder_ready_ = false;
    parent_->buffer_ = nullptr;
    this->temp_buffer_.clear();
  }

  int decode(unsigned char *data, unsigned int length) override {
    ESP_LOGD(TAG, "WebPDecoder::decode called with %u bytes", length);

    if (!this->size_set_) {
      temp_buffer_.insert(temp_buffer_.end(), data, data + length);
      ESP_LOGD(TAG, "WebPDecoder::decode: Accumulated %zu bytes for WebP info.", temp_buffer_.size());

      int width = 0, height = 0;
      if (WebPGetInfo(temp_buffer_.data(), temp_buffer_.size(), &width, &height)) {
        ESP_LOGI(TAG, "WebPDecoder::decode: Image dimensions obtained: %u x %u", width, height);
        this->set_size(width, height);

        if (parent_->buffer_ == nullptr) {
          ESP_LOGE(TAG, "WebPDecoder::decode: Image buffer not allocated in OnlineImage. Cannot proceed with decoding.");
          return -1;
        }

        if (!WebPInitDecoderConfig(&config_)) {
          ESP_LOGE(TAG, "WebPDecoder::decode: Failed to initialize WebP decoder configuration.");
          return -1;
        }

        // Use buffer_width_ and buffer_height_ for stride and size
        config_.output.colorspace = MODE_BGRA;  // Try MODE_BGRA if colors are swapped
        config_.output.u.RGBA.rgba = parent_->buffer_;
        config_.output.u.RGBA.stride = parent_->buffer_width_ * 4;
        config_.output.u.RGBA.size = parent_->buffer_width_ * parent_->buffer_height_ * 4;

        webp_decoder_ = WebPINewDecoder(&config_.output);
        if (webp_decoder_ == nullptr) {
          ESP_LOGE(TAG, "WebPDecoder::decode: Failed to initialize WebP incremental decoder.");
          return -1;
        }

        this->decoder_ready_ = true;
        this->size_set_ = true;
        ESP_LOGI(TAG, "WebPDecoder::decode: Decoder initialized and ready.");

        VP8StatusCode status = WebPIAppend(webp_decoder_, temp_buffer_.data(), temp_buffer_.size());
        if (status != VP8_STATUS_OK && status != VP8_STATUS_SUSPENDED) {
          ESP_LOGE(TAG, "WebPDecoder::decode: Error decoding WebP image during initial append, status code: %d", status);
          WebPIDelete(webp_decoder_);
          webp_decoder_ = nullptr;
          return -1;
        }

        ESP_LOGD(TAG, "WebPDecoder::decode: Decoded initial %zu bytes.", temp_buffer_.size());
        decoded_bytes_ += temp_buffer_.size();
        temp_buffer_.clear();

        return length;
      }

      ESP_LOGD(TAG, "WebPDecoder::decode: Insufficient data to determine image dimensions.");
      return 0;
    }

    if (!this->decoder_ready_) {
      ESP_LOGE(TAG, "WebPDecoder::decode: Decoder not ready. Ensure that prepare() was called and succeeded.");
      return -1;
    }

    VP8StatusCode status = WebPIAppend(webp_decoder_, data, length);
    if (status == VP8_STATUS_OK || status == VP8_STATUS_SUSPENDED) {
      ESP_LOGD(TAG, "WebPDecoder::decode: Successfully decoded %u bytes.", length);
      decoded_bytes_ += length;
      return length;
    } else {
      ESP_LOGE(TAG, "WebPDecoder::decode: Error decoding WebP image, status code: %d", status);
      WebPIDelete(webp_decoder_);
      webp_decoder_ = nullptr;
      return -1;
    }
  }

  bool is_finished() {
    ESP_LOGD(TAG, "WebPDecoder::is_finished called.");

    if (webp_decoder_ == nullptr) {
      ESP_LOGD(TAG, "WebPDecoder::is_finished: WebP decoder is null, decoding considered finished.");
      return true;
    }

    int left, top, width, height;
    if (WebPIDecodedArea(webp_decoder_, &left, &top, &width, &height)) {
      ESP_LOGD(TAG, "WebPDecoder::is_finished: Decoded area (%d, %d) - Width: %d, Height: %d", left, top, width, height);
      if (width == parent_->buffer_width_ && height == parent_->buffer_height_) {
        ESP_LOGI(TAG, "WebPDecoder::is_finished: WebP image decoding complete.");
        WebPIDelete(webp_decoder_);
        webp_decoder_ = nullptr;
        return true;
      }
    }

    ESP_LOGD(TAG, "WebPDecoder::is_finished: Decoding not yet complete.");
    return false;
  }

 private:
  OnlineImage *parent_;
  bool decoder_ready_;
  bool size_set_;
  WebPIDecoder *webp_decoder_;
  WebPDecoderConfig config_;
  std::vector<uint8_t> temp_buffer_;
};
}  // namespace online_image
}  // namespace esphome
