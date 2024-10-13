#pragma once

#include "image_decoder.h"
#include "esphome/core/log.h"
#include "webp/decode.h"  // Include libwebp's decode header

namespace esphome {
namespace online_image {

static const char *const TAG = "online_image.webp";

/**
 * @brief Image decoder specialization for WebP images using libwebp.
 */
class WebPDecoder : public ImageDecoder {
 public:
  WebPDecoder(OnlineImage *image) : ImageDecoder(image), decoded_(false) {}
  ~WebPDecoder() override = default;

  void prepare(uint32_t download_size) override {
    ImageDecoder::prepare(download_size);
    decoded_ = false;
  }

  /**
   * @brief Decode the WebP image using libwebp.
   *
   * This implementation uses WebPDecodeRGBA to decode the entire image at once.
   *
   * @param buffer Pointer to the downloaded image data.
   * @param size   Size of the downloaded data.
   * @return int   Number of bytes processed or negative on error.
   */
  int decode(uint8_t *buffer, size_t size) override {
    if (decoded_) {
      // Already decoded
      return 0;
    }

    ESP_LOGD(TAG, "Content-Length: %d", this->download_size_);
    ESP_LOGD(TAG, "Downloaded size: %d", size);

    int width = 0, height = 0;
    ESP_LOGD(TAG, "Starting WebP decoding...");
    uint8_t *rgba_data = WebPDecodeRGBA(buffer, size, &width, &height);
    if (!rgba_data) {
      ESP_LOGE(TAG, "Error decoding WebP image. Buffer size: %d", size);
      return -1;
    }
    
    ESP_LOGD(TAG, "WebP decoding successful. Image dimensions: %dx%d", width, height);

    ESP_LOGD(TAG, "Decoded WebP image: %dx%d", width, height);

    // Resize the image buffer using ImageDecoder's set_size method
    this->set_size(width, height);

    // Draw the decoded RGBA data into the image buffer using the draw method
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        int idx = (y * width + x) * 4;
        Color color(rgba_data[idx], rgba_data[idx + 1], rgba_data[idx + 2], rgba_data[idx + 3]);
        this->draw(x, y, 1, 1, color);  // Use the draw method instead of direct access to draw_pixel_
      }
    }

    // Free the decoded RGBA data
    WebPFree(rgba_data);

    decoded_ = true;
    this->decoded_bytes_ = this->download_size_;  // Mark as fully decoded

    return size;  // All bytes processed
  }

 private:
  bool decoded_;
};

}  // namespace online_image
}  // namespace esphome
