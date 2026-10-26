/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "config.h"

#include "client.h"
#include "link.h"

#include <guacamole/client.h>
#include <guacamole/display.h>
#include <guacamole/user.h>

#include <stdint.h>

/* MEA input event types (PROTOCOL.md §4, little-endian fields) */
#define GUAC_MEA_IN_KEY        0x01
#define GUAC_MEA_IN_BUTTON     0x02
#define GUAC_MEA_IN_MOTION_ABS 0x04
#define GUAC_MEA_IN_WHEEL      0x05

/* One wheel notch */
#define GUAC_MEA_WHEEL_NOTCH 120

static void guac_mea_put16(unsigned char* out, unsigned int value) {
    out[0] = value & 0xFF;
    out[1] = (value >> 8) & 0xFF;
}

int guac_mea_encode_key(unsigned char* out, int pressed, int keysym) {
    uint32_t k = (uint32_t) keysym;
    out[0] = GUAC_MEA_IN_KEY;
    out[1] = pressed ? 1 : 0;
    out[2] = k & 0xFF;
    out[3] = (k >> 8) & 0xFF;
    out[4] = (k >> 16) & 0xFF;
    out[5] = (k >> 24) & 0xFF;
    return 6;
}

int guac_mea_encode_button(unsigned char* out, int pressed, int button) {
    out[0] = GUAC_MEA_IN_BUTTON;
    out[1] = pressed ? 1 : 0;
    out[2] = (unsigned char) button;
    return 3;
}

int guac_mea_encode_motion_abs(unsigned char* out, int x, int y,
        int width, int height) {

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= width) x = width - 1;
    if (y >= height) y = height - 1;

    out[0] = GUAC_MEA_IN_MOTION_ABS;
    guac_mea_put16(out + 1, width > 1 ? (unsigned int) ((int64_t) x * 65535 / (width - 1)) : 0);
    guac_mea_put16(out + 3, height > 1 ? (unsigned int) ((int64_t) y * 65535 / (height - 1)) : 0);
    return 5;

}

int guac_mea_encode_wheel(unsigned char* out, int dx, int dy) {
    out[0] = GUAC_MEA_IN_WHEEL;
    guac_mea_put16(out + 1, (unsigned int) (int16_t) dx);
    guac_mea_put16(out + 3, (unsigned int) (int16_t) dy);
    return 5;
}

int guac_mea_user_mouse_handler(guac_user* user, int x, int y, int mask) {

    guac_client* client = user->client;
    guac_mea_client* mea_client = (guac_mea_client*) client->data;
    guac_mea_link* link = &mea_client->link;
    unsigned char event[8];

    if (mea_client->render_thread != NULL)
        guac_display_render_thread_notify_user_moved_mouse(mea_client->render_thread,
                user, x, y, mask);

    if (mea_client->width <= 0 || mea_client->height <= 0)
        return 0;

    guac_mea_link_write(link, GUAC_MEA_LINK_INPUT, event,
            guac_mea_encode_motion_abs(event, x, y, mea_client->width, mea_client->height));

    /* Left, middle, right: Guacamole mask bits 0..2 = X buttons 1..3 */
    int changed = mask ^ mea_client->button_mask;
    for (int bit = 0; bit < 3; bit++) {
        if (changed & (1 << bit))
            guac_mea_link_write(link, GUAC_MEA_LINK_INPUT, event,
                    guac_mea_encode_button(event, mask & (1 << bit), bit + 1));
    }

    /* Scroll up/down arrive as press of mask bits 3/4 */
    int pressed = mask & ~mea_client->button_mask;
    if (pressed & GUAC_CLIENT_MOUSE_SCROLL_UP)
        guac_mea_link_write(link, GUAC_MEA_LINK_INPUT, event,
                guac_mea_encode_wheel(event, 0, -GUAC_MEA_WHEEL_NOTCH));
    if (pressed & GUAC_CLIENT_MOUSE_SCROLL_DOWN)
        guac_mea_link_write(link, GUAC_MEA_LINK_INPUT, event,
                guac_mea_encode_wheel(event, 0, GUAC_MEA_WHEEL_NOTCH));

    mea_client->button_mask = mask;
    return 0;

}

int guac_mea_user_key_handler(guac_user* user, int keysym, int pressed) {

    guac_mea_client* mea_client = (guac_mea_client*) user->client->data;
    unsigned char event[8];

    guac_mea_link_write(&mea_client->link, GUAC_MEA_LINK_INPUT, event,
            guac_mea_encode_key(event, pressed, keysym));

    return 0;

}
