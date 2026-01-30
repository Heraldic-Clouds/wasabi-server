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

#ifndef GUAC_GAME_SETTINGS_H
#define GUAC_GAME_SETTINGS_H

#include <guacamole/user.h>

#include <stdbool.h>

/**
 * Default X11 display to capture.
 */
#define GUAC_GAME_DEFAULT_DISPLAY ":0"

/**
 * Default framerate for streaming (frames per second).
 */
#define GUAC_GAME_DEFAULT_FRAMERATE 60

/**
 * Default bitrate for encoding (in kbps).
 */
#define GUAC_GAME_DEFAULT_BITRATE 15000

/**
 * Default NVENC preset for encoding.
 */
#define GUAC_GAME_DEFAULT_PRESET "p1"

/**
 * Default codec for encoding.
 */
#define GUAC_GAME_DEFAULT_CODEC "h264"

/**
 * The client argument array for the game protocol.
 */
extern const char* GUAC_GAME_CLIENT_ARGS[];

/**
 * Settings for the game streaming protocol.
 */
typedef struct guac_game_settings {

    /**
     * The X11 display to capture (e.g., ":0").
     */
    char* display;

    /**
     * Target framerate for streaming.
     */
    int framerate;

    /**
     * Target bitrate for encoding in kbps.
     */
    int bitrate;

    /**
     * The codec to use ("h264" or "hevc").
     */
    char* codec;

    /**
     * NVENC preset for encoding quality/speed tradeoff.
     * Ranges from "p1" (fastest/lowest quality) to "p7" (slowest/best quality).
     */
    char* preset;

    /**
     * Whether to use constant bitrate mode.
     */
    bool cbr;

    /**
     * Whether the connection is read-only.
     */
    bool read_only;

    /**
     * Width override (0 = use display width).
     */
    int width;

    /**
     * Height override (0 = use display height).
     */
    int height;

    /**
     * Whether to force lossless compression on image layers.
     */
    bool lossless;

} guac_game_settings;

/**
 * Parses the arguments given to the game protocol client, returning a
 * guac_game_settings structure containing all parameter values. If an error
 * occurs, NULL is returned.
 *
 * @param user
 *     The user joining the connection.
 *
 * @param argc
 *     The number of arguments provided.
 *
 * @param argv
 *     The argument values.
 *
 * @return
 *     A newly-allocated guac_game_settings structure containing the parsed
 *     values, or NULL if an error occurs.
 */
guac_game_settings* guac_game_parse_args(guac_user* user,
        int argc, const char** argv);

/**
 * Frees the given guac_game_settings structure and all associated memory.
 *
 * @param settings
 *     The settings structure to free.
 */
void guac_game_settings_free(guac_game_settings* settings);

#endif /* GUAC_GAME_SETTINGS_H */
