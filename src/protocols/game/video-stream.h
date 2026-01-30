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

#ifndef GUAC_GAME_VIDEO_STREAM_H
#define GUAC_GAME_VIDEO_STREAM_H

#include <guacamole/client.h>
#include <guacamole/protocol.h>
#include <guacamole/socket.h>

#include <stdbool.h>

/**
 * Video stream context for sending encoded frames to Guacamole clients.
 */
typedef struct guac_game_video_stream {

    /**
     * The associated guac_client.
     */
    guac_client* client;

    /**
     * The layer index used for video.
     */
    int layer_index;

    /**
     * The stream index for video data.
     */
    int stream_index;

    /**
     * Video width.
     */
    int width;

    /**
     * Video height.
     */
    int height;

    /**
     * Video mimetype.
     */
    const char* mimetype;

    /**
     * Whether the stream has been initialized with the client.
     */
    bool initialized;

    /**
     * Current timestamp for video frames.
     */
    guac_timestamp timestamp;

} guac_game_video_stream;

/**
 * Initializes a video stream for sending encoded video to clients.
 *
 * @param stream
 *     The video stream context to initialize.
 *
 * @param client
 *     The guac_client to stream to.
 *
 * @param layer_index
 *     The layer index for the video.
 *
 * @param width
 *     Video width in pixels.
 *
 * @param height
 *     Video height in pixels.
 *
 * @param mimetype
 *     The video mimetype (e.g., "video/h264").
 *
 * @return
 *     Zero on success, non-zero on failure.
 */
int guac_game_video_stream_init(guac_game_video_stream* stream,
        guac_client* client, int layer_index,
        int width, int height, const char* mimetype);

/**
 * Sends an encoded video frame to all connected clients.
 *
 * @param stream
 *     The video stream context.
 *
 * @param data
 *     The encoded frame data.
 *
 * @param size
 *     Size of the encoded data in bytes.
 *
 * @param is_keyframe
 *     Whether this is a keyframe.
 *
 * @return
 *     Zero on success, non-zero on failure.
 */
int guac_game_video_stream_write(guac_game_video_stream* stream,
        const unsigned char* data, int size, bool is_keyframe);

/**
 * Sends a resize notification to indicate the video dimensions have changed.
 *
 * @param stream
 *     The video stream context.
 *
 * @param width
 *     New video width in pixels.
 *
 * @param height
 *     New video height in pixels.
 *
 * @return
 *     Zero on success, non-zero on failure.
 */
int guac_game_video_stream_resize(guac_game_video_stream* stream,
        int width, int height);

/**
 * Cleans up the video stream.
 *
 * @param stream
 *     The video stream context to clean up.
 */
void guac_game_video_stream_cleanup(guac_game_video_stream* stream);

#endif /* GUAC_GAME_VIDEO_STREAM_H */
