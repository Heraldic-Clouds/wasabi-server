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

#include "settings.h"

#include <guacamole/mem.h>
#include <guacamole/user.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Client plugin arguments */
const char* GUAC_GAME_CLIENT_ARGS[] = {
    "display",
    "framerate",
    "bitrate",
    "codec",
    "preset",
    "cbr",
    "read-only",
    "width",
    "height",
    "force-lossless",
    NULL
};

enum GAME_ARGS_IDX {

    /**
     * The X11 display to capture (e.g., ":0").
     */
    IDX_DISPLAY,

    /**
     * Target framerate for streaming.
     */
    IDX_FRAMERATE,

    /**
     * Target bitrate for encoding in kbps.
     */
    IDX_BITRATE,

    /**
     * The codec to use ("h264" or "hevc").
     */
    IDX_CODEC,

    /**
     * NVENC preset for encoding.
     */
    IDX_PRESET,

    /**
     * Whether to use constant bitrate mode.
     */
    IDX_CBR,

    /**
     * Whether the connection is read-only.
     */
    IDX_READ_ONLY,

    /**
     * Width override.
     */
    IDX_WIDTH,

    /**
     * Height override.
     */
    IDX_HEIGHT,

    /**
     * Whether to force lossless compression.
     */
    IDX_FORCE_LOSSLESS,

    GAME_ARGS_COUNT
};

guac_game_settings* guac_game_parse_args(guac_user* user,
        int argc, const char** argv) {

    /* Validate arg count */
    if (argc != GAME_ARGS_COUNT) {
        guac_user_log(user, GUAC_LOG_WARNING, "Incorrect number of connection "
                "parameters provided: expected %i, got %i.",
                GAME_ARGS_COUNT, argc);
        return NULL;
    }

    guac_game_settings* settings = guac_mem_zalloc(sizeof(guac_game_settings));

    /* Display to capture */
    settings->display =
        guac_user_parse_args_string(user, GUAC_GAME_CLIENT_ARGS, argv,
                IDX_DISPLAY, GUAC_GAME_DEFAULT_DISPLAY);

    /* Framerate */
    settings->framerate =
        guac_user_parse_args_int(user, GUAC_GAME_CLIENT_ARGS, argv,
                IDX_FRAMERATE, GUAC_GAME_DEFAULT_FRAMERATE);

    /* Validate framerate */
    if (settings->framerate < 1 || settings->framerate > 240) {
        guac_user_log(user, GUAC_LOG_WARNING,
                "Invalid framerate %d, using default %d",
                settings->framerate, GUAC_GAME_DEFAULT_FRAMERATE);
        settings->framerate = GUAC_GAME_DEFAULT_FRAMERATE;
    }

    /* Bitrate */
    settings->bitrate =
        guac_user_parse_args_int(user, GUAC_GAME_CLIENT_ARGS, argv,
                IDX_BITRATE, GUAC_GAME_DEFAULT_BITRATE);

    /* Validate bitrate */
    if (settings->bitrate < 500 || settings->bitrate > 100000) {
        guac_user_log(user, GUAC_LOG_WARNING,
                "Invalid bitrate %d kbps, using default %d kbps",
                settings->bitrate, GUAC_GAME_DEFAULT_BITRATE);
        settings->bitrate = GUAC_GAME_DEFAULT_BITRATE;
    }

    /* Codec */
    settings->codec =
        guac_user_parse_args_string(user, GUAC_GAME_CLIENT_ARGS, argv,
                IDX_CODEC, GUAC_GAME_DEFAULT_CODEC);

    /* Validate codec */
    if (strcmp(settings->codec, "h264") != 0 &&
        strcmp(settings->codec, "hevc") != 0) {
        guac_user_log(user, GUAC_LOG_WARNING,
                "Invalid codec '%s', using default '%s'",
                settings->codec, GUAC_GAME_DEFAULT_CODEC);
        guac_mem_free(settings->codec);
        settings->codec = guac_mem_strdup(GUAC_GAME_DEFAULT_CODEC);
    }

    /* Preset */
    settings->preset =
        guac_user_parse_args_string(user, GUAC_GAME_CLIENT_ARGS, argv,
                IDX_PRESET, GUAC_GAME_DEFAULT_PRESET);

    /* CBR mode */
    settings->cbr =
        guac_user_parse_args_boolean(user, GUAC_GAME_CLIENT_ARGS, argv,
                IDX_CBR, true);

    /* Read-only mode */
    settings->read_only =
        guac_user_parse_args_boolean(user, GUAC_GAME_CLIENT_ARGS, argv,
                IDX_READ_ONLY, false);

    /* Width override */
    settings->width =
        guac_user_parse_args_int(user, GUAC_GAME_CLIENT_ARGS, argv,
                IDX_WIDTH, 0);

    /* Height override */
    settings->height =
        guac_user_parse_args_int(user, GUAC_GAME_CLIENT_ARGS, argv,
                IDX_HEIGHT, 0);

    /* Force lossless */
    settings->lossless =
        guac_user_parse_args_boolean(user, GUAC_GAME_CLIENT_ARGS, argv,
                IDX_FORCE_LOSSLESS, false);

    return settings;

}

void guac_game_settings_free(guac_game_settings* settings) {

    /* Free string settings */
    guac_mem_free(settings->display);
    guac_mem_free(settings->codec);
    guac_mem_free(settings->preset);

    /* Free settings structure */
    guac_mem_free(settings);

}
