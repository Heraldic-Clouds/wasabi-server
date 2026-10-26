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

#include <guacamole/client.h>
#include <guacamole/display.h>
#include <guacamole/mem.h>
#include <guacamole/string.h>
#include <guacamole/user.h>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

const char* GUAC_MEA_CLIENT_ARGS[] = {
    "hostname",
    "port",
    "token",
    "profile",
    "fps",
    "kbps",
    "read-only",
    NULL
};

enum GUAC_MEA_ARGS_IDX {
    IDX_HOSTNAME,
    IDX_PORT,
    IDX_TOKEN,
    IDX_PROFILE,
    IDX_FPS,
    IDX_KBPS,
    IDX_READ_ONLY,
    MEA_ARGS_COUNT
};

guac_mea_settings* guac_mea_parse_args(guac_user* user, int argc,
        const char** argv) {

    if (argc != MEA_ARGS_COUNT) {
        guac_user_log(user, GUAC_LOG_WARNING, "Incorrect number of "
                "connection parameters provided: expected %i, got %i.",
                MEA_ARGS_COUNT, argc);
        return NULL;
    }

    guac_mea_settings* settings = guac_mem_zalloc(sizeof(guac_mea_settings));

    settings->hostname = guac_user_parse_args_string(user,
            GUAC_MEA_CLIENT_ARGS, argv, IDX_HOSTNAME, "");
    settings->port = guac_user_parse_args_string(user,
            GUAC_MEA_CLIENT_ARGS, argv, IDX_PORT, GUAC_MEA_DEFAULT_PORT);
    settings->token = guac_user_parse_args_string(user,
            GUAC_MEA_CLIENT_ARGS, argv, IDX_TOKEN, "");
    settings->profile = guac_user_parse_args_string(user,
            GUAC_MEA_CLIENT_ARGS, argv, IDX_PROFILE, "balanced");
    settings->fps = guac_user_parse_args_int(user,
            GUAC_MEA_CLIENT_ARGS, argv, IDX_FPS, 30);
    settings->kbps = guac_user_parse_args_int(user,
            GUAC_MEA_CLIENT_ARGS, argv, IDX_KBPS, 8000);
    settings->read_only = guac_user_parse_args_boolean(user,
            GUAC_MEA_CLIENT_ARGS, argv, IDX_READ_ONLY, 0);

    /* The token is embedded in a JSON string: refuse anything that is not
     * the base64url.base64url form instead of escaping it */
    for (const char* c = settings->token; *c; c++) {
        if (!(strchr("-_.", *c) || (*c >= '0' && *c <= '9')
                    || (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z'))) {
            guac_user_log(user, GUAC_LOG_WARNING, "Malformed MEA stream token.");
            guac_mea_settings_free(settings);
            return NULL;
        }
    }

    if (strcmp(settings->profile, "economy") && strcmp(settings->profile, "balanced")
            && strcmp(settings->profile, "high")) {
        guac_mem_free(settings->profile);
        settings->profile = guac_strdup("balanced");
    }

    if (settings->hostname[0] == '\0' || settings->token[0] == '\0') {
        guac_user_log(user, GUAC_LOG_WARNING,
                "The \"hostname\" and \"token\" parameters are required.");
        guac_mea_settings_free(settings);
        return NULL;
    }

    return settings;

}

void guac_mea_settings_free(guac_mea_settings* settings) {

    if (settings == NULL)
        return;

    guac_mem_free(settings->hostname);
    guac_mem_free(settings->port);
    guac_mem_free(settings->token);
    guac_mem_free(settings->profile);
    guac_mem_free(settings);

}

static int guac_mea_join_pending_handler(guac_client* client) {

    guac_mea_client* mea_client = (guac_mea_client*) client->data;

    if (mea_client->display != NULL) {
        guac_display_dup(mea_client->display, client->pending_socket);
        guac_socket_flush(client->pending_socket);
    }

    return 0;

}

static int guac_mea_user_join_handler(guac_user* user, int argc, char** argv) {

    guac_mea_client* mea_client = (guac_mea_client*) user->client->data;

    guac_mea_settings* settings = guac_mea_parse_args(user, argc,
            (const char**) argv);
    if (settings == NULL)
        return 1;

    user->data = settings;

    if (user->owner) {
        mea_client->settings = settings;
        mea_client->running = 1;
        if (pthread_create(&mea_client->stream_thread, NULL,
                    guac_mea_client_thread, user->client)) {
            guac_user_log(user, GUAC_LOG_ERROR,
                    "Unable to start MEA stream thread.");
            return 1;
        }
        mea_client->thread_started = 1;
    }

    if (!settings->read_only) {
        user->mouse_handler = guac_mea_user_mouse_handler;
        user->key_handler = guac_mea_user_key_handler;
    }

    return 0;

}

static int guac_mea_user_leave_handler(guac_user* user) {

    guac_mea_client* mea_client = (guac_mea_client*) user->client->data;

    if (mea_client->display != NULL)
        guac_display_notify_user_left(mea_client->display, user);

    if (!user->owner)
        guac_mea_settings_free((guac_mea_settings*) user->data);

    return 0;

}

static int guac_mea_client_free_handler(guac_client* client) {

    guac_mea_client* mea_client = (guac_mea_client*) client->data;

    mea_client->running = 0;

    if (mea_client->display != NULL)
        guac_display_stop(mea_client->display);

    if (mea_client->thread_started)
        pthread_join(mea_client->stream_thread, NULL);

    /* Destroyed here, not by the stream thread, as input handlers use it */
    if (mea_client->render_thread != NULL)
        guac_display_render_thread_destroy(mea_client->render_thread);

    if (mea_client->display != NULL)
        guac_display_free(mea_client->display);

    guac_mea_link_destroy(&mea_client->link);
    guac_mea_settings_free(mea_client->settings);
    guac_mem_free(mea_client);

    return 0;

}

int guac_client_init(guac_client* client) {

    client->args = GUAC_MEA_CLIENT_ARGS;

    guac_mea_client* mea_client = guac_mem_zalloc(sizeof(guac_mea_client));
    guac_mea_link_init(&mea_client->link);
    client->data = mea_client;

    client->join_handler = guac_mea_user_join_handler;
    client->join_pending_handler = guac_mea_join_pending_handler;
    client->leave_handler = guac_mea_user_leave_handler;
    client->free_handler = guac_mea_client_free_handler;

    return 0;

}
