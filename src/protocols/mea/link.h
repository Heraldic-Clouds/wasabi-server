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

#ifndef GUAC_MEA_LINK_H
#define GUAC_MEA_LINK_H

/*
 * The MEA Stream agent link (Type3-hypervisor-manager stream/docs/PROTOCOL.md
 * section 6): TCP frames of len:u32be | kind:u8 | payload, where len counts
 * the kind byte. guacd plays the gateway's role towards one agent.
 */

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#define GUAC_MEA_LINK_HELLO   0x01
#define GUAC_MEA_LINK_READY   0x02
#define GUAC_MEA_LINK_VIDEO   0x10
#define GUAC_MEA_LINK_AUDIO   0x11
#define GUAC_MEA_LINK_INPUT   0x20
#define GUAC_MEA_LINK_CONTROL 0x21
#define GUAC_MEA_LINK_STATS   0x30
#define GUAC_MEA_LINK_BYE     0x7F

/**
 * Largest frame accepted from an agent (4 MiB, as in the protocol).
 */
#define GUAC_MEA_LINK_MAX_FRAME (4 << 20)

/**
 * Interval between heartbeats. Agents drop a link silent for 3.5 s and
 * release all held input.
 */
#define GUAC_MEA_LINK_PING_MS 1000

typedef struct guac_mea_link {

    /**
     * Connected socket, or -1.
     */
    int fd;

    /**
     * Serializes writes from the stream thread and the user input threads.
     */
    pthread_mutex_t write_lock;

    /**
     * Receive buffer holding the most recent frame payload.
     */
    unsigned char* buffer;

    /**
     * Allocated size of buffer.
     */
    size_t buffer_size;

} guac_mea_link;

/**
 * Initializes an unconnected link. Call once before any other function.
 */
void guac_mea_link_init(guac_mea_link* link);

/**
 * Connects to an agent. Returns zero on success.
 */
int guac_mea_link_connect(guac_mea_link* link, const char* host,
        const char* port);

/**
 * Writes one frame. Safe to call from any thread. Returns zero on success.
 */
int guac_mea_link_write(guac_mea_link* link, uint8_t kind,
        const void* payload, size_t length);

/**
 * Reads one frame into link->buffer, sending a heartbeat whenever the agent
 * is quiet for GUAC_MEA_LINK_PING_MS. Returns zero on success with *kind and
 * *length set, or non-zero when the link failed or *running became zero.
 */
int guac_mea_link_read(guac_mea_link* link, uint8_t* kind, size_t* length,
        volatile int* running);

/**
 * Closes the link and frees its buffer. Later writes fail harmlessly.
 */
void guac_mea_link_close(guac_mea_link* link);

/**
 * Closes the link and releases its lock. No thread may use it afterwards.
 */
void guac_mea_link_destroy(guac_mea_link* link);

/**
 * Finds the integer value of a top-level "key": in a flat JSON object.
 * Returns def when the key is absent. Agent messages are small, flat and
 * produced by our own code, so a full JSON parser is not needed.
 */
long guac_mea_json_int(const char* json, size_t length, const char* key,
        long def);

#endif
