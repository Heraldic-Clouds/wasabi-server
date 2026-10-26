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

#include "link.h"

#include <guacamole/mem.h>
#include <guacamole/timestamp.h>

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int guac_mea_link_connect(guac_mea_link* link, const char* host,
        const char* port) {

    struct addrinfo hints = { .ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM };
    struct addrinfo* addresses;

    if (getaddrinfo(host, port, &hints, &addresses))
        return 1;

    for (struct addrinfo* a = addresses; a != NULL; a = a->ai_next) {

        int fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd < 0)
            continue;

        if (connect(fd, a->ai_addr, a->ai_addrlen) == 0) {
            int one = 1;
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
            link->fd = fd;
            break;
        }

        close(fd);

    }

    freeaddrinfo(addresses);
    return link->fd < 0;

}

/**
 * Writes all of buffer, retrying on short writes. Caller holds write_lock.
 */
static int guac_mea_write_all(int fd, const unsigned char* buffer, size_t length) {

    while (length > 0) {
        ssize_t written = send(fd, buffer, length, MSG_NOSIGNAL);
        if (written < 0) {
            if (errno == EINTR)
                continue;
            return 1;
        }
        buffer += written;
        length -= written;
    }

    return 0;

}

int guac_mea_link_write(guac_mea_link* link, uint8_t kind,
        const void* payload, size_t length) {

    if (length + 1 > GUAC_MEA_LINK_MAX_FRAME)
        return 1;

    uint32_t size = (uint32_t) length + 1;
    unsigned char header[5] = {
        size >> 24, size >> 16, size >> 8, size, kind
    };

    pthread_mutex_lock(&link->write_lock);
    int result = link->fd < 0
              || guac_mea_write_all(link->fd, header, sizeof(header))
              || guac_mea_write_all(link->fd, payload, length);
    pthread_mutex_unlock(&link->write_lock);

    return result;

}

/**
 * Reads exactly length bytes, pinging the agent while it is quiet.
 */
static int guac_mea_read_full(guac_mea_link* link, unsigned char* buffer,
        size_t length, volatile int* running) {

    static const char ping[] = "{\"t\":\"ping\"}";
    guac_timestamp last_ping = guac_timestamp_current();

    while (length > 0) {

        if (!*running)
            return 1;

        if (guac_timestamp_current() - last_ping >= GUAC_MEA_LINK_PING_MS) {
            if (guac_mea_link_write(link, GUAC_MEA_LINK_CONTROL, ping, sizeof(ping) - 1))
                return 1;
            last_ping = guac_timestamp_current();
        }

        struct pollfd p = { .fd = link->fd, .events = POLLIN };
        int ready = poll(&p, 1, 250);
        if (ready < 0 && errno != EINTR)
            return 1;
        if (ready <= 0)
            continue;

        ssize_t received = recv(link->fd, buffer, length, 0);
        if (received == 0)
            return 1;
        if (received < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return 1;
        }

        buffer += received;
        length -= received;

    }

    return 0;

}

int guac_mea_link_read(guac_mea_link* link, uint8_t* kind, size_t* length,
        volatile int* running) {

    unsigned char header[5];
    if (guac_mea_read_full(link, header, sizeof(header), running))
        return 1;

    uint32_t size = ((uint32_t) header[0] << 24) | ((uint32_t) header[1] << 16)
                  | ((uint32_t) header[2] << 8) | header[3];
    if (size < 1 || size > GUAC_MEA_LINK_MAX_FRAME)
        return 1;

    size_t payload = size - 1;
    if (payload + 1 > link->buffer_size) {
        guac_mem_free(link->buffer);
        link->buffer_size = payload + 1;
        link->buffer = guac_mem_alloc(link->buffer_size);
    }

    if (guac_mea_read_full(link, link->buffer, payload, running))
        return 1;

    link->buffer[payload] = '\0'; /* JSON payloads can be read as strings */
    *kind = header[4];
    *length = payload;
    return 0;

}

void guac_mea_link_init(guac_mea_link* link) {
    link->fd = -1;
    link->buffer = NULL;
    link->buffer_size = 0;
    pthread_mutex_init(&link->write_lock, NULL);
}

void guac_mea_link_close(guac_mea_link* link) {

    /* Under the write lock so input threads never write to a reused fd */
    pthread_mutex_lock(&link->write_lock);
    if (link->fd >= 0) {
        shutdown(link->fd, SHUT_RDWR);
        close(link->fd);
        link->fd = -1;
    }
    pthread_mutex_unlock(&link->write_lock);

    guac_mem_free(link->buffer);
    link->buffer = NULL;
    link->buffer_size = 0;

}

void guac_mea_link_destroy(guac_mea_link* link) {
    guac_mea_link_close(link);
    pthread_mutex_destroy(&link->write_lock);
}

long guac_mea_json_int(const char* json, size_t length, const char* key,
        long def) {

    char pattern[64];
    int n = snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    if (n < 0 || (size_t) n >= sizeof(pattern))
        return def;

    const char* end = json + length;
    for (const char* p = json; p + n <= end; p++) {
        if (memcmp(p, pattern, n) == 0) {
            char* parsed;
            long value = strtol(p + n, &parsed, 10);
            return parsed == p + n ? def : value;
        }
    }

    return def;

}
