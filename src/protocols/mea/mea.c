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
#include <guacamole/protocol-constants.h>
#include <guacamole/rect.h>

#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>

#include <stdio.h>
#include <string.h>

/**
 * Sends the HELLO frame. Only H.264 is offered: it is the codec every agent
 * encoder supports and the one libavcodec decodes everywhere.
 */
static int guac_mea_send_hello(guac_mea_client* mea_client) {

    guac_mea_settings* settings = mea_client->settings;
    char hello[4096];

    int length = snprintf(hello, sizeof(hello),
            "{\"token\":\"%s\",\"codecs\":[\"h264\"],\"profile\":\"%s\","
            "\"fps\":%i,\"kbps\":%i}",
            settings->token, settings->profile, settings->fps, settings->kbps);

    if (length < 0 || (size_t) length >= sizeof(hello))
        return 1;

    return guac_mea_link_write(&mea_client->link, GUAC_MEA_LINK_HELLO, hello, length);

}

/**
 * Aborts the connection for an agent BYE, telling token refusals apart.
 */
static void guac_mea_abort_bye(guac_client* client, const char* json, size_t length) {

    long code = guac_mea_json_int(json, length, "code", 1000);

    if (code == GUAC_MEA_CODE_BAD_TOKEN || code == GUAC_MEA_CODE_FORBIDDEN)
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_CLIENT_UNAUTHORIZED,
                "The MEA stream token was refused (%li).", code);
    else if (code == 1000)
        guac_client_stop(client);
    else
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_ERROR,
                "The desktop agent ended the stream (%li).", code);

}

/**
 * Draws one decoded frame to the default layer.
 */
static void guac_mea_draw(guac_mea_client* mea_client, AVFrame* frame,
        struct SwsContext** sws) {

    guac_display_layer* layer = guac_display_default_layer(mea_client->display);

    if (frame->width != mea_client->width || frame->height != mea_client->height) {
        mea_client->width = frame->width;
        mea_client->height = frame->height;
        guac_display_layer_resize(layer, frame->width, frame->height);
    }

    *sws = sws_getCachedContext(*sws, frame->width, frame->height, frame->format,
            frame->width, frame->height, AV_PIX_FMT_BGRA, SWS_POINT, NULL, NULL, NULL);
    if (*sws == NULL)
        return;

    guac_display_layer_raw_context* context = guac_display_layer_open_raw(layer);

    uint8_t* dst[4] = { context->buffer };
    int dst_stride[4] = { (int) context->stride };
    sws_scale(*sws, (const uint8_t* const*) frame->data, frame->linesize,
            0, frame->height, dst, dst_stride);

    guac_rect_init(&context->dirty, 0, 0, frame->width, frame->height);
    guac_display_layer_close_raw(layer, context);

    guac_display_render_thread_notify_frame(mea_client->render_thread);

}

void* guac_mea_client_thread(void* data) {

    guac_client* client = (guac_client*) data;
    guac_mea_client* mea_client = (guac_mea_client*) client->data;
    guac_mea_settings* settings = mea_client->settings;
    guac_mea_link* link = &mea_client->link;

    AVCodecContext* decoder = NULL;
    AVPacket* packet = NULL;
    AVFrame* frame = NULL;
    struct SwsContext* sws = NULL;

    guac_client_log(client, GUAC_LOG_INFO, "Connecting to MEA agent %s:%s",
            settings->hostname, settings->port);

    if (guac_mea_link_connect(link, settings->hostname, settings->port)) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_NOT_FOUND,
                "Unable to reach the desktop agent.");
        goto done;
    }

    if (guac_mea_send_hello(mea_client)) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_ERROR,
                "Unable to send the stream request.");
        goto done;
    }

    /* The token is single use; forget it once sent */
    memset(settings->token, 0, strlen(settings->token));

    /* Wait for READY (or BYE when the token is refused) */
    uint8_t kind;
    size_t length;
    if (guac_mea_link_read(link, &kind, &length, &mea_client->running)) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_ERROR,
                "The desktop agent closed the connection.");
        goto done;
    }

    if (kind == GUAC_MEA_LINK_BYE) {
        guac_mea_abort_bye(client, (const char*) link->buffer, length);
        goto done;
    }

    if (kind != GUAC_MEA_LINK_READY) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_ERROR,
                "Unexpected reply from the desktop agent.");
        goto done;
    }

    mea_client->width = guac_mea_json_int((const char*) link->buffer, length, "w", 1280);
    mea_client->height = guac_mea_json_int((const char*) link->buffer, length, "h", 720);
    guac_client_log(client, GUAC_LOG_INFO, "MEA stream ready: %ix%i",
            mea_client->width, mea_client->height);

    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    decoder = codec ? avcodec_alloc_context3(codec) : NULL;
    if (decoder == NULL) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_SERVER_ERROR,
                "No H.264 decoder available.");
        goto done;
    }

    decoder->flags |= AV_CODEC_FLAG_LOW_DELAY;
    decoder->thread_count = 1; /* frame threading would add a frame of delay */
    if (avcodec_open2(decoder, codec, NULL) < 0) {
        guac_client_abort(client, GUAC_PROTOCOL_STATUS_SERVER_ERROR,
                "Unable to open the H.264 decoder.");
        goto done;
    }

    packet = av_packet_alloc();
    frame = av_frame_alloc();

    mea_client->display = guac_display_alloc(client);
    guac_display_layer_resize(guac_display_default_layer(mea_client->display),
            mea_client->width, mea_client->height);
    guac_display_set_cursor(mea_client->display, GUAC_DISPLAY_CURSOR_POINTER);
    guac_display_end_frame(mea_client->display);
    mea_client->render_thread = guac_display_render_thread_create(mea_client->display);

    /* Ask for a keyframe so decoding can start immediately */
    static const char keyframe[] = "{\"t\":\"keyframe\"}";
    guac_mea_link_write(link, GUAC_MEA_LINK_CONTROL, keyframe, sizeof(keyframe) - 1);

    while (client->state == GUAC_CLIENT_RUNNING && mea_client->running) {

        if (guac_mea_link_read(link, &kind, &length, &mea_client->running)) {
            if (mea_client->running)
                guac_client_abort(client, GUAC_PROTOCOL_STATUS_UPSTREAM_ERROR,
                        "Lost the connection to the desktop agent.");
            break;
        }

        if (kind == GUAC_MEA_LINK_BYE) {
            guac_mea_abort_bye(client, (const char*) link->buffer, length);
            break;
        }

        /* Audio (Opus) is not forwarded by this adapter */
        if (kind != GUAC_MEA_LINK_VIDEO || length < 9)
            continue;

        packet->data = link->buffer + 9; /* flags:u8 | pts:u64be | AU */
        packet->size = (int) (length - 9);

        if (avcodec_send_packet(decoder, packet) < 0) {
            static const char resync[] = "{\"t\":\"keyframe\"}";
            guac_mea_link_write(link, GUAC_MEA_LINK_CONTROL, resync, sizeof(resync) - 1);
            continue;
        }

        while (avcodec_receive_frame(decoder, frame) == 0) {
            guac_mea_draw(mea_client, frame, &sws);
            av_frame_unref(frame);
        }

    }

done:

    if (link->fd >= 0) {
        static const char bye[] = "{\"code\":1000,\"reason\":\"guacamole disconnect\"}";
        guac_mea_link_write(link, GUAC_MEA_LINK_BYE, bye, sizeof(bye) - 1);
    }

    sws_freeContext(sws);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&decoder);
    guac_mea_link_close(link);

    guac_client_stop(client);
    return NULL;

}
