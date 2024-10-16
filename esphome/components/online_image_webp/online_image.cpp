#include "online_image.h"

#include "esphome/core/log.h"
#include "esphome/components/image/image.h"  // Include for ImageType

static const char *const TAG = "online_image";

#include "image_decoder.h"

#ifdef USE_ONLINE_IMAGE_PNG_SUPPORT
#include "png_image.h"
#endif

#ifdef USE_ONLINE_IMAGE_WEBP_SUPPORT
#include "webp_image.h"
#endif

namespace esphome {
namespace online_image {

using image::ImageType;

// Rest of the code remains the same

}  // namespace online_image
}  // namespace esphome
