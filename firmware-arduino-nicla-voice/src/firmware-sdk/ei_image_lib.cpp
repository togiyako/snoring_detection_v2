/* The Clear BSD License
 *
 * Copyright (c) 2025 EdgeImpulse Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 *   * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

// set to 1 to generate and send a test image
#define SEND_TEST_IMAGE 0

#include "firmware-sdk/ei_camera_interface.h"
#include "firmware-sdk/ei_device_info_lib.h"

#if EI_PORTING_SONY_SPRESENSE
#include "malloc.h" //for memalign
#endif

#include <memory>

#include "edge-impulse-sdk/dsp/ei_utils.h"
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "edge-impulse-sdk/porting/ei_classifier_porting.h"
#include "firmware-sdk/at_base64_lib.h"
#include "firmware-sdk/ei_device_interface.h"
#include "firmware-sdk/ei_image_lib.h"

// *********************************** AT cmd functions ***************

static void respond_and_change_to_max_baud()
{
    auto device = EiDeviceInfo::get_device();
    // sleep a little to let the daemon attach on the new baud rate...
    ei_printf("\r\nOK");
    ei_sleep(100);
    device->set_max_data_output_baudrate();
    ei_sleep(100);
}

static void change_to_normal_baud()
{
    auto device = EiDeviceInfo::get_device();
    // lower baud rate
    ei_printf("\r\nOK\r\n");
    ei_sleep(100);
    device->set_default_data_output_baudrate();
    // sleep a little to let the daemon attach on baud rate 115200 again...
    ei_sleep(100);
}

static bool ei_camera_take_snapshot_encode_and_output_no_init(size_t width, size_t height)
{
    using namespace ei::image::processing;

    bool needs_a_resize = false;

    ei_device_snapshot_resolutions_t fb_resoluton;

    uint16_t final_width = width;
    uint16_t final_height = height;

    auto camera = EiCamera::get_camera();

    // check if minimum suitable sensor resolution is the same as 
    // desired snapshot resolution
    // if not we need to resize later
    fb_resoluton = camera->search_resolution(width, height);

    if (width != fb_resoluton.width || height != fb_resoluton.height) {
        needs_a_resize = true;
        width = fb_resoluton.width;
        height = fb_resoluton.height;
    }

    // rgb888 packed, 3B color depth
    uint32_t size = width * height * RGB888_B_SIZE;

#if EI_PORTING_SONY_SPRESENSE
    // 32 BYTE aligned (for Sony, maybe others too?  Monster vector moves in our future?)
    auto image_p = std::unique_ptr<uint8_t, decltype(free) *> { reinterpret_cast<uint8_t *>(
                                                                    memalign(32, size)),
                                                                free };
#else // more portable version
    std::unique_ptr<uint8_t[]> image_p(new uint8_t[size]);
#endif

    if (!image_p) {
        ei_printf("Take snapshot: Out of memory\n");
        return false;
    }

    auto image = image_p.get();

#if SEND_TEST_IMAGE
    uint32_t counter = 0;
    for (int i = 0; i < size; i += 3) {
        image[i] = counter & 0xff;
        image[i + 1] = counter >> 8;
        image[i + 2] = counter >> 16;
        counter += 100;
    }
#else
    bool isOK = camera->ei_camera_capture_rgb888_packed_big_endian(image, size);
    if (!isOK) {
        return false;
    }

    if (needs_a_resize) {
        // interpolate in place
        ei::image::processing::crop_and_interpolate_rgb888(
            image,
            width,
            height,
            image,
            final_width,
            final_height);
    }

#endif

    // recalculate size b/c now we want to send just the interpolated bytes
    base64_encode(
        reinterpret_cast<char *>(image),
        final_height * final_width * RGB888_B_SIZE,
        ei_putchar);

    return true;
}

extern bool
ei_camera_take_snapshot_output_on_serial(size_t width, size_t height, bool use_max_baudrate)
{
    auto camera = EiCamera::get_camera();

    // sets camera sensor resolution to the best suitable 
    // might not be the same as final snapshot resolution
    // this is why below we pass desired snapshot resolution
    // to ei_camera_take_snapshot_encode_and_output_no_init
    if (!camera->init(width, height)) {
        ei_printf("Failed to init camera\n");
        return false;
    }

    if (use_max_baudrate) {
        respond_and_change_to_max_baud();
    }

    // here we pass desired snapshot resolution 
    // if it is different from camera sensor resolution
    // we will resize before sending out the image
    bool isOK = ei_camera_take_snapshot_encode_and_output_no_init(width, height);
    camera->deinit();

    if (use_max_baudrate) {
        change_to_normal_baud();
    }
    else {
        ei_printf("\r\nOK\r\n");
    }

    return isOK;
}

extern bool ei_camera_start_snapshot_stream(size_t width, size_t height, bool use_max_baudrate)
{
    bool isOK = true;
    ei_printf("Starting snapshot stream...\n");
    auto camera = EiCamera::get_camera();

    if (!camera->init(width, height)) {
        ei_printf("Failed to init camera\n");
        return false;
    }

    if (use_max_baudrate) {
        respond_and_change_to_max_baud();
    }

    while (!ei_user_invoke_stop_lib()) {
        isOK &= ei_camera_take_snapshot_encode_and_output_no_init(width, height);
        ei_printf("\r\n");
    }
    camera->deinit();

    if (use_max_baudrate) {
        change_to_normal_baud();
    }

    return isOK;
}
