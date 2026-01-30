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

#include "capture-x11.h"
#include "client.h"
#include "game.h"
#include "nvenc.h"
#include "settings.h"
#include "video-stream.h"

#include <guacamole/client.h>
#include <guacamole/display.h>
#include <guacamole/timestamp.h>

#include <pthread.h>
#include <stdlib.h>
#include <time.h>

/**
 * Capture context for X11 screen grabbing.
 */
static guac_game_x11_capture capture;

/**
 * NVENC encoder context.
 */
static guac_game_nvenc_encoder encoder;

/**
 * Video stream context.
 */
static guac_game_video_stream video_stream;

int guac_game_init_pipeline(guac_client* client) {

    guac_game_client* game_client = (guac_game_client*) client->data;
    guac_game_settings* settings = game_client->settings;

    /* Initialize X11 capture */
    if (guac_game_x11_capture_init(&capture, settings->display) != 0) {
        guac_client_log(client, GUAC_LOG_ERROR,
                "Failed to initialize X11 capture for display %s",
                settings->display);
        return -1;
    }

    /* Store X11 display in client for input injection */
    game_client->x11_display = capture.display;

    /* Get capture dimensions */
    int width = guac_game_x11_capture_get_width(&capture);
    int height = guac_game_x11_capture_get_height(&capture);

    /* Use overrides if specified */
    if (settings->width > 0)
        width = settings->width;
    if (settings->height > 0)
        height = settings->height;

    game_client->width = width;
    game_client->height = height;

    guac_client_log(client, GUAC_LOG_INFO,
            "Game streaming: %dx%d @ %d fps, %d kbps, codec=%s, preset=%s",
            width, height, settings->framerate, settings->bitrate,
            settings->codec, settings->preset);

    /* Initialize NVENC encoder */
    if (guac_game_nvenc_init(&encoder, width, height,
                settings->framerate, settings->bitrate,
                settings->codec, settings->preset) != 0) {
        guac_client_log(client, GUAC_LOG_ERROR,
                "Failed to initialize NVENC encoder");
        guac_game_x11_capture_cleanup(&capture);
        return -1;
    }

    game_client->encoder = &encoder;
    game_client->video_mimetype = guac_game_nvenc_get_mimetype(&encoder);

    /* Initialize video stream */
    if (guac_game_video_stream_init(&video_stream, client, 0,
                width, height, game_client->video_mimetype) != 0) {
        guac_client_log(client, GUAC_LOG_ERROR,
                "Failed to initialize video stream");
        guac_game_nvenc_cleanup(&encoder);
        guac_game_x11_capture_cleanup(&capture);
        return -1;
    }

    return 0;

}

void guac_game_cleanup_pipeline(guac_client* client) {

    guac_game_client* game_client = (guac_game_client*) client->data;

    /* Clean up video stream */
    guac_game_video_stream_cleanup(&video_stream);

    /* Clean up encoder */
    guac_game_nvenc_cleanup(&encoder);

    /* Clean up capture */
    guac_game_x11_capture_cleanup(&capture);

    game_client->encoder = NULL;
    game_client->x11_display = NULL;

}

/**
 * Calculates the time to sleep to maintain target framerate.
 *
 * @param start_time
 *     The start time of the frame in nanoseconds.
 *
 * @param frame_duration_ns
 *     Target frame duration in nanoseconds.
 *
 * @return
 *     The time to sleep in nanoseconds.
 */
static long calculate_sleep_time(guac_timestamp start_time, long frame_duration_ns) {

    guac_timestamp current = guac_timestamp_current();
    long elapsed_ns = (current - start_time) * 1000000;  /* Convert ms to ns */
    long sleep_time = frame_duration_ns - elapsed_ns;

    return sleep_time > 0 ? sleep_time : 0;

}

void* guac_game_client_thread(void* data) {

    guac_client* client = (guac_client*) data;
    guac_game_client* game_client = (guac_game_client*) client->data;
    guac_game_settings* settings = game_client->settings;

    /* Initialize the streaming pipeline */
    if (guac_game_init_pipeline(client) != 0) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_ERROR,
                "Failed to initialize game streaming pipeline");
        return NULL;
    }

    /* Create display for Guacamole */
    game_client->display = guac_display_alloc(client);
    guac_display_layer_resize(guac_display_default_layer(game_client->display),
            game_client->width, game_client->height);

    /* Use lossless compression if requested */
    guac_display_layer_set_lossless(
            guac_display_default_layer(game_client->display),
            settings->lossless);

    /* Set cursor */
    guac_display_set_cursor(game_client->display, GUAC_DISPLAY_CURSOR_POINTER);

    guac_display_end_frame(game_client->display);

    /* Start render thread */
    game_client->render_thread = guac_display_render_thread_create(game_client->display);

    /* Calculate frame timing */
    long frame_duration_ns = 1000000000L / settings->framerate;

    /* Mark as running */
    game_client->running = 1;

    guac_client_log(client, GUAC_LOG_INFO, "Game streaming started");

    /* Main capture/encode/stream loop */
    while (client->state == GUAC_CLIENT_RUNNING && game_client->running) {

        guac_timestamp frame_start = guac_timestamp_current();

        /* Capture frame */
        unsigned char* frame_data = guac_game_x11_capture_frame(&capture);
        if (frame_data == NULL) {
            guac_client_log(client, GUAC_LOG_WARNING,
                    "Failed to capture frame");
            continue;
        }

        /* Calculate stride (BGRA = 4 bytes per pixel) */
        int stride = game_client->width * 4;

        /* Encode frame */
        unsigned char* encoded_data;
        int encoded_size;
        bool is_keyframe;

        int ret = guac_game_nvenc_encode(&encoder, frame_data, stride,
                &encoded_data, &encoded_size, &is_keyframe);

        if (ret == 0 && encoded_size > 0) {

            /* Send encoded frame to clients */
            guac_game_video_stream_write(&video_stream,
                    encoded_data, encoded_size, is_keyframe);

        }
        else if (ret < 0) {
            guac_client_log(client, GUAC_LOG_WARNING,
                    "Failed to encode frame");
        }

        /* Maintain target framerate */
        long sleep_ns = calculate_sleep_time(frame_start, frame_duration_ns);
        if (sleep_ns > 0) {
            struct timespec sleep_time;
            sleep_time.tv_sec = sleep_ns / 1000000000L;
            sleep_time.tv_nsec = sleep_ns % 1000000000L;
            nanosleep(&sleep_time, NULL);
        }

    }

    guac_client_log(client, GUAC_LOG_INFO, "Game streaming stopped");

    /* Stop render thread */
    if (game_client->render_thread != NULL) {
        guac_display_render_thread_destroy(game_client->render_thread);
        game_client->render_thread = NULL;
    }

    /* Stop client */
    guac_client_stop(client);

    return NULL;

}
