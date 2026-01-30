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

#ifndef GUAC_GAME_CAPTURE_X11_H
#define GUAC_GAME_CAPTURE_X11_H

#include "client.h"

#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>
#include <sys/shm.h>

/**
 * X11 capture context for screen grabbing.
 */
typedef struct guac_game_x11_capture {

    /**
     * X11 display connection.
     */
    Display* display;

    /**
     * Root window of the display.
     */
    Window root;

    /**
     * Screen number.
     */
    int screen;

    /**
     * XShm segment info for shared memory capture.
     */
    XShmSegmentInfo shm_info;

    /**
     * XImage for storing captured frames.
     */
    XImage* image;

    /**
     * Width of the capture area.
     */
    int width;

    /**
     * Height of the capture area.
     */
    int height;

    /**
     * Whether XShm is available and being used.
     */
    int use_shm;

} guac_game_x11_capture;

/**
 * Initializes X11 screen capture using XShm for efficient screen grabbing.
 *
 * @param capture
 *     The capture context to initialize.
 *
 * @param display_name
 *     The X11 display name (e.g., ":0").
 *
 * @return
 *     Zero on success, non-zero on failure.
 */
int guac_game_x11_capture_init(guac_game_x11_capture* capture,
        const char* display_name);

/**
 * Captures a single frame from the X11 display.
 *
 * @param capture
 *     The capture context.
 *
 * @return
 *     Pointer to the captured frame data (BGRA format), or NULL on failure.
 *     The memory is owned by the capture context and should not be freed.
 */
unsigned char* guac_game_x11_capture_frame(guac_game_x11_capture* capture);

/**
 * Gets the width of the captured frames.
 *
 * @param capture
 *     The capture context.
 *
 * @return
 *     The width in pixels.
 */
int guac_game_x11_capture_get_width(guac_game_x11_capture* capture);

/**
 * Gets the height of the captured frames.
 *
 * @param capture
 *     The capture context.
 *
 * @return
 *     The height in pixels.
 */
int guac_game_x11_capture_get_height(guac_game_x11_capture* capture);

/**
 * Cleans up X11 capture resources.
 *
 * @param capture
 *     The capture context to clean up.
 */
void guac_game_x11_capture_cleanup(guac_game_x11_capture* capture);

#endif /* GUAC_GAME_CAPTURE_X11_H */
