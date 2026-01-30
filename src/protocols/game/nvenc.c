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

#include "nvenc.h"

#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>

#include <stdlib.h>
#include <string.h>

int guac_game_nvenc_init(guac_game_nvenc_encoder* encoder,
        int width, int height, int framerate, int bitrate,
        const char* codec, const char* preset) {

    memset(encoder, 0, sizeof(guac_game_nvenc_encoder));

    encoder->width = width;
    encoder->height = height;
    encoder->framerate = framerate;
    encoder->bitrate = bitrate;
    encoder->pts = 0;
    encoder->is_hevc = (strcmp(codec, "hevc") == 0);

    /* Find NVENC encoder */
    const char* encoder_name = encoder->is_hevc ? "hevc_nvenc" : "h264_nvenc";
    encoder->codec = avcodec_find_encoder_by_name(encoder_name);

    if (encoder->codec == NULL) {
        /* Fall back to software encoder */
        encoder_name = encoder->is_hevc ? "libx265" : "libx264";
        encoder->codec = avcodec_find_encoder_by_name(encoder_name);

        if (encoder->codec == NULL) {
            return -1;
        }
    }

    /* Allocate codec context */
    encoder->codec_ctx = avcodec_alloc_context3(encoder->codec);
    if (encoder->codec_ctx == NULL) {
        return -1;
    }

    /* Configure encoder for low latency */
    encoder->codec_ctx->width = width;
    encoder->codec_ctx->height = height;
    encoder->codec_ctx->time_base = (AVRational){1, framerate};
    encoder->codec_ctx->framerate = (AVRational){framerate, 1};
    encoder->codec_ctx->pix_fmt = AV_PIX_FMT_NV12;
    encoder->codec_ctx->bit_rate = bitrate * 1000;
    encoder->codec_ctx->rc_max_rate = bitrate * 1000;
    encoder->codec_ctx->rc_buffer_size = bitrate * 1000 / 2;
    encoder->codec_ctx->gop_size = framerate * 2;  /* Keyframe every 2 seconds */
    encoder->codec_ctx->max_b_frames = 0;  /* No B-frames for low latency */
    encoder->codec_ctx->thread_count = 1;

    /* Set NVENC-specific options for ultra-low latency */
    if (strstr(encoder->codec->name, "nvenc") != NULL) {
        av_opt_set(encoder->codec_ctx->priv_data, "preset", preset, 0);
        av_opt_set(encoder->codec_ctx->priv_data, "tune", "ull", 0);
        av_opt_set(encoder->codec_ctx->priv_data, "rc", "cbr", 0);
        av_opt_set(encoder->codec_ctx->priv_data, "delay", "0", 0);
        av_opt_set(encoder->codec_ctx->priv_data, "zerolatency", "1", 0);
        av_opt_set_int(encoder->codec_ctx->priv_data, "cbr", 1, 0);
    }
    else if (strstr(encoder->codec->name, "x264") != NULL) {
        /* libx264 fallback settings */
        av_opt_set(encoder->codec_ctx->priv_data, "preset", "ultrafast", 0);
        av_opt_set(encoder->codec_ctx->priv_data, "tune", "zerolatency", 0);
    }
    else if (strstr(encoder->codec->name, "x265") != NULL) {
        /* libx265 fallback settings */
        av_opt_set(encoder->codec_ctx->priv_data, "preset", "ultrafast", 0);
        av_opt_set(encoder->codec_ctx->priv_data, "tune", "zerolatency", 0);
    }

    /* Open encoder */
    if (avcodec_open2(encoder->codec_ctx, encoder->codec, NULL) < 0) {
        avcodec_free_context(&encoder->codec_ctx);
        return -1;
    }

    /* Allocate frame */
    encoder->frame = av_frame_alloc();
    if (encoder->frame == NULL) {
        avcodec_free_context(&encoder->codec_ctx);
        return -1;
    }

    encoder->frame->format = encoder->codec_ctx->pix_fmt;
    encoder->frame->width = width;
    encoder->frame->height = height;

    if (av_frame_get_buffer(encoder->frame, 0) < 0) {
        av_frame_free(&encoder->frame);
        avcodec_free_context(&encoder->codec_ctx);
        return -1;
    }

    /* Allocate packet */
    encoder->packet = av_packet_alloc();
    if (encoder->packet == NULL) {
        av_frame_free(&encoder->frame);
        avcodec_free_context(&encoder->codec_ctx);
        return -1;
    }

    /* Create scaler for BGRA to NV12 conversion */
    encoder->sws_ctx = sws_getContext(
        width, height, AV_PIX_FMT_BGRA,
        width, height, AV_PIX_FMT_NV12,
        SWS_FAST_BILINEAR, NULL, NULL, NULL
    );

    if (encoder->sws_ctx == NULL) {
        av_packet_free(&encoder->packet);
        av_frame_free(&encoder->frame);
        avcodec_free_context(&encoder->codec_ctx);
        return -1;
    }

    encoder->initialized = true;
    return 0;

}

int guac_game_nvenc_encode(guac_game_nvenc_encoder* encoder,
        const unsigned char* bgra_data, int bgra_stride,
        unsigned char** out_data, int* out_size, bool* is_keyframe) {

    if (!encoder->initialized)
        return -1;

    int ret;

    /* Make frame writable */
    if ((ret = av_frame_make_writable(encoder->frame)) < 0)
        return -1;

    /* Convert BGRA to NV12 */
    const uint8_t* src_slices[1] = { bgra_data };
    int src_stride[1] = { bgra_stride };

    sws_scale(
        encoder->sws_ctx,
        src_slices, src_stride,
        0, encoder->height,
        encoder->frame->data, encoder->frame->linesize
    );

    /* Set timestamp */
    encoder->frame->pts = encoder->pts++;

    /* Send frame to encoder */
    ret = avcodec_send_frame(encoder->codec_ctx, encoder->frame);
    if (ret < 0)
        return -1;

    /* Receive encoded packet */
    ret = avcodec_receive_packet(encoder->codec_ctx, encoder->packet);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return 1;  /* No packet ready yet */
    }
    else if (ret < 0) {
        return -1;
    }

    /* Return encoded data */
    *out_data = encoder->packet->data;
    *out_size = encoder->packet->size;
    *is_keyframe = (encoder->packet->flags & AV_PKT_FLAG_KEY) != 0;

    return 0;

}

void guac_game_nvenc_force_keyframe(guac_game_nvenc_encoder* encoder) {
    if (encoder->initialized && encoder->frame != NULL) {
        encoder->frame->pict_type = AV_PICTURE_TYPE_I;
    }
}

const char* guac_game_nvenc_get_mimetype(guac_game_nvenc_encoder* encoder) {
    return encoder->is_hevc ? "video/hevc" : "video/h264";
}

void guac_game_nvenc_cleanup(guac_game_nvenc_encoder* encoder) {

    if (encoder->sws_ctx != NULL) {
        sws_freeContext(encoder->sws_ctx);
        encoder->sws_ctx = NULL;
    }

    if (encoder->packet != NULL) {
        av_packet_free(&encoder->packet);
        encoder->packet = NULL;
    }

    if (encoder->frame != NULL) {
        av_frame_free(&encoder->frame);
        encoder->frame = NULL;
    }

    if (encoder->codec_ctx != NULL) {
        avcodec_free_context(&encoder->codec_ctx);
        encoder->codec_ctx = NULL;
    }

    encoder->initialized = false;

}
