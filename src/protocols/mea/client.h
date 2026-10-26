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

#ifndef GUAC_MEA_CLIENT_H
#define GUAC_MEA_CLIENT_H

/*
 * libguac-client-mea (experimental): shows an Eleet desktop through the MEA
 * Stream agent instead of VNC/RDP. guacd connects to the agent inside the
 * desktop container exactly as the WebRTC gateway does, authenticates with a
 * short-lived MEA stream token, decodes the agent's H.264 and draws it to the
 * Guacamole display; mouse and keyboard go back as MEA input events.
 *
 * It exists to compare the agent pipeline inside Guacamole. Gaming Mode in
 * the browser uses WebRTC and is the supported path; VNC remains the
 * standard desktop.
 */

#include "link.h"

#include <guacamole/client.h>
#include <guacamole/display.h>
#include <guacamole/user.h>

#include <pthread.h>

/**
 * Default agent port.
 */
#define GUAC_MEA_DEFAULT_PORT "47100"

/**
 * Agent link error codes that mean the token was refused.
 */
#define GUAC_MEA_CODE_BAD_TOKEN 4001
#define GUAC_MEA_CODE_FORBIDDEN 4003

extern const char* GUAC_MEA_CLIENT_ARGS[];

typedef struct guac_mea_settings {

    /**
     * Agent host (the desktop container's address on the host bridge).
     */
    char* hostname;

    /**
     * Agent port.
     */
    char* port;

    /**
     * MEA stream token minted by the MEA API for this desktop. Single use.
     */
    char* token;

    /**
     * Requested profile: economy, balanced or high (clamped by the token).
     */
    char* profile;

    /**
     * Requested frame rate.
     */
    int fps;

    /**
     * Requested bitrate in kbps.
     */
    int kbps;

    /**
     * Whether input is ignored.
     */
    int read_only;

} guac_mea_settings;

typedef struct guac_mea_client {

    /**
     * Owner's settings.
     */
    guac_mea_settings* settings;

    /**
     * Link to the agent.
     */
    guac_mea_link link;

    /**
     * Display and its render thread.
     */
    guac_display* display;
    guac_display_render_thread* render_thread;

    /**
     * Stream thread: link, decode, draw.
     */
    pthread_t stream_thread;
    int thread_started;

    /**
     * Cleared to stop the stream thread.
     */
    volatile int running;

    /**
     * Size of the remote picture, for scaling pointer positions.
     */
    int width;
    int height;

    /**
     * Last Guacamole mouse button mask, to send changes only.
     */
    int button_mask;

} guac_mea_client;

guac_mea_settings* guac_mea_parse_args(guac_user* user, int argc,
        const char** argv);

void guac_mea_settings_free(guac_mea_settings* settings);

/**
 * The stream thread.
 */
void* guac_mea_client_thread(void* data);

guac_user_mouse_handler guac_mea_user_mouse_handler;
guac_user_key_handler guac_mea_user_key_handler;

/**
 * Encodes one MEA input event (PROTOCOL.md §4) for the agent link.
 * Returns the encoded length; exported for tests.
 */
int guac_mea_encode_key(unsigned char* out, int pressed, int keysym);
int guac_mea_encode_button(unsigned char* out, int pressed, int button);
int guac_mea_encode_motion_abs(unsigned char* out, int x, int y,
        int width, int height);
int guac_mea_encode_wheel(unsigned char* out, int dx, int dy);

#endif
