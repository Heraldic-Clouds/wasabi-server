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

#include "video-stream.h"

#include <guacamole/client.h>
#include <guacamole/layer.h>
#include <guacamole/protocol.h>
#include <guacamole/socket.h>
#include <guacamole/stream.h>
#include <guacamole/timestamp.h>

#include <stdlib.h>
#include <string.h>

/**
 * Maximum blob size for video data (6KB to stay under protocol limits).
 */
#define GUAC_GAME_VIDEO_BLOB_SIZE 6144

int guac_game_video_stream_init(guac_game_video_stream* stream,
        guac_client* client, int layer_index,
        int width, int height, const char* mimetype) {

    stream->client = client;
    stream->layer_index = layer_index;
    stream->width = width;
    stream->height = height;
    stream->mimetype = mimetype;
    stream->initialized = false;
    stream->stream_index = 0;
    stream->timestamp = guac_timestamp_current();

    return 0;

}

/**
 * Sends video instruction to begin a new video stream on a layer.
 *
 * @param socket
 *     The socket to send the instruction on.
 *
 * @param layer_index
 *     The layer index.
 *
 * @param stream_index
 *     The stream index.
 *
 * @param mimetype
 *     The video mimetype.
 *
 * @return
 *     Zero on success, non-zero on failure.
 */
static int guac_game_send_video_instruction(guac_socket* socket,
        int layer_index, int stream_index, const char* mimetype) {

    /* Send video instruction in the format:
     * 5.video,<layer>,<stream>,<mimetype>; */
    return
        guac_socket_write_string(socket, "5.video,")
        || guac_socket_write_int(socket, layer_index)
        || guac_socket_write_string(socket, ",")
        || guac_socket_write_int(socket, stream_index)
        || guac_socket_write_string(socket, ",")
        || guac_socket_write_int(socket, strlen(mimetype))
        || guac_socket_write_string(socket, ".")
        || guac_socket_write_string(socket, mimetype)
        || guac_socket_write_string(socket, ";");

}

int guac_game_video_stream_write(guac_game_video_stream* stream,
        const unsigned char* data, int size, bool is_keyframe) {

    guac_client* client = stream->client;
    guac_socket* socket = client->socket;

    /* Initialize stream on first frame */
    if (!stream->initialized) {

        /* Use the default layer (index 0) */
        guac_layer* layer = GUAC_DEFAULT_LAYER;

        /* First resize the layer to match video dimensions */
        guac_protocol_send_size(socket, layer, stream->width, stream->height);

        /* Allocate a stream for video data */
        guac_stream* guac_stream = guac_client_alloc_stream(client);
        stream->stream_index = guac_stream->index;

        /* Send video instruction */
        guac_game_send_video_instruction(socket, stream->layer_index,
                stream->stream_index, stream->mimetype);

        stream->initialized = true;

    }

    /* Send video data in blob chunks */
    int offset = 0;
    while (offset < size) {

        int chunk_size = size - offset;
        if (chunk_size > GUAC_GAME_VIDEO_BLOB_SIZE)
            chunk_size = GUAC_GAME_VIDEO_BLOB_SIZE;

        /* Send blob instruction */
        guac_protocol_send_blob(socket,
                (guac_stream*)(intptr_t)stream->stream_index,
                data + offset, chunk_size);

        offset += chunk_size;

    }

    /* Flush socket */
    guac_socket_flush(socket);

    /* Update timestamp */
    stream->timestamp = guac_timestamp_current();

    return 0;

}

int guac_game_video_stream_resize(guac_game_video_stream* stream,
        int width, int height) {

    guac_client* client = stream->client;
    guac_socket* socket = client->socket;

    stream->width = width;
    stream->height = height;

    /* Send size instruction */
    guac_protocol_send_size(socket, GUAC_DEFAULT_LAYER, width, height);
    guac_socket_flush(socket);

    return 0;

}

void guac_game_video_stream_cleanup(guac_game_video_stream* stream) {

    if (stream->initialized && stream->client != NULL) {

        guac_socket* socket = stream->client->socket;

        /* Send end instruction to close stream */
        guac_protocol_send_end(socket,
                (guac_stream*)(intptr_t)stream->stream_index);
        guac_socket_flush(socket);

    }

    stream->initialized = false;

}
