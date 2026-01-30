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

#ifndef GUAC_GAME_NVENC_H
#define GUAC_GAME_NVENC_H

#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include <stdbool.h>

/**
 * NVENC encoder context for hardware-accelerated video encoding.
 */
typedef struct guac_game_nvenc_encoder {

    /**
     * FFmpeg codec context for NVENC.
     */
    AVCodecContext* codec_ctx;

    /**
     * The AVCodec being used.
     */
    const AVCodec* codec;

    /**
     * AVFrame for holding input frame data.
     */
    AVFrame* frame;

    /**
     * AVPacket for holding encoded data.
     */
    AVPacket* packet;

    /**
     * Software scaler context for format conversion.
     */
    struct SwsContext* sws_ctx;

    /**
     * Width of the video.
     */
    int width;

    /**
     * Height of the video.
     */
    int height;

    /**
     * Target framerate.
     */
    int framerate;

    /**
     * Target bitrate in kbps.
     */
    int bitrate;

    /**
     * Presentation timestamp counter.
     */
    int64_t pts;

    /**
     * Whether encoder is initialized.
     */
    bool initialized;

    /**
     * Whether the codec is H.265/HEVC (true) or H.264 (false).
     */
    bool is_hevc;

} guac_game_nvenc_encoder;

/**
 * Initializes the NVENC encoder with the given parameters.
 *
 * @param encoder
 *     The encoder context to initialize.
 *
 * @param width
 *     Video width in pixels.
 *
 * @param height
 *     Video height in pixels.
 *
 * @param framerate
 *     Target framerate.
 *
 * @param bitrate
 *     Target bitrate in kbps.
 *
 * @param codec
 *     Codec name ("h264" or "hevc").
 *
 * @param preset
 *     NVENC preset ("p1" to "p7").
 *
 * @return
 *     Zero on success, non-zero on failure.
 */
int guac_game_nvenc_init(guac_game_nvenc_encoder* encoder,
        int width, int height, int framerate, int bitrate,
        const char* codec, const char* preset);

/**
 * Encodes a single frame of BGRA image data.
 *
 * @param encoder
 *     The encoder context.
 *
 * @param bgra_data
 *     Pointer to BGRA pixel data.
 *
 * @param bgra_stride
 *     Stride (bytes per row) of the BGRA data.
 *
 * @param out_data
 *     Pointer to receive the encoded packet data.
 *
 * @param out_size
 *     Pointer to receive the encoded packet size.
 *
 * @param is_keyframe
 *     Pointer to receive whether this is a keyframe.
 *
 * @return
 *     Zero on success, positive if no packet ready yet, negative on error.
 */
int guac_game_nvenc_encode(guac_game_nvenc_encoder* encoder,
        const unsigned char* bgra_data, int bgra_stride,
        unsigned char** out_data, int* out_size, bool* is_keyframe);

/**
 * Forces the encoder to produce a keyframe on the next encode call.
 *
 * @param encoder
 *     The encoder context.
 */
void guac_game_nvenc_force_keyframe(guac_game_nvenc_encoder* encoder);

/**
 * Gets the codec mimetype string for use with Guacamole protocol.
 *
 * @param encoder
 *     The encoder context.
 *
 * @return
 *     The mimetype string (e.g., "video/h264" or "video/hevc").
 */
const char* guac_game_nvenc_get_mimetype(guac_game_nvenc_encoder* encoder);

/**
 * Cleans up the NVENC encoder and frees all resources.
 *
 * @param encoder
 *     The encoder context to clean up.
 */
void guac_game_nvenc_cleanup(guac_game_nvenc_encoder* encoder);

#endif /* GUAC_GAME_NVENC_H */
