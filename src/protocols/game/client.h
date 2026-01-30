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

#ifndef GUAC_GAME_CLIENT_H
#define GUAC_GAME_CLIENT_H

#include "config.h"
#include "settings.h"

#include <guacamole/client.h>
#include <guacamole/display.h>

#include <pthread.h>

/**
 * The maximum time to wait for screen capture data, in milliseconds.
 */
#define GUAC_GAME_FRAME_TIMEOUT 1000

/**
 * The default framerate to use for streaming if not specified.
 */
#define GUAC_GAME_DEFAULT_FRAMERATE 60

/**
 * Game streaming client data.
 */
typedef struct guac_game_client {

    /**
     * Game streaming settings.
     */
    guac_game_settings* settings;

    /**
     * The guac_display this client uses to render the desktop.
     */
    guac_display* display;

    /**
     * The render thread for the display.
     */
    guac_display_render_thread* render_thread;

    /**
     * The main game capture/encode thread.
     */
    pthread_t game_thread;

    /**
     * X11 display connection.
     */
    void* x11_display;

    /**
     * X11 root window.
     */
    unsigned long x11_root_window;

    /**
     * XShm image for screen capture.
     */
    void* xshm_image;

    /**
     * XShm segment info.
     */
    void* xshm_info;

    /**
     * Current display width.
     */
    int width;

    /**
     * Current display height.
     */
    int height;

    /**
     * NVENC encoder context.
     */
    void* encoder;

    /**
     * Video stream layer index.
     */
    int video_layer_index;

    /**
     * Video stream mimetype.
     */
    const char* video_mimetype;

    /**
     * Mutex for thread-safe operations.
     */
    pthread_mutex_t lock;

    /**
     * Flag indicating whether the client is running.
     */
    int running;

} guac_game_client;

/**
 * Handler which frees all data associated with the guac_client.
 */
guac_client_free_handler guac_game_client_free_handler;

#endif /* GUAC_GAME_CLIENT_H */
