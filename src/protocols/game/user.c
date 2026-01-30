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
#include "game.h"
#include "input.h"
#include "settings.h"
#include "user.h"

#include <guacamole/client.h>
#include <guacamole/display.h>
#include <guacamole/socket.h>
#include <guacamole/user.h>

#include <pthread.h>

int guac_game_user_join_handler(guac_user* user, int argc, char** argv) {

    guac_game_client* game_client = (guac_game_client*) user->client->data;

    /* Parse provided arguments */
    guac_game_settings* settings = guac_game_parse_args(user,
            argc, (const char**) argv);

    /* Fail if settings cannot be parsed */
    if (settings == NULL) {
        guac_user_log(user, GUAC_LOG_INFO,
                "Badly formatted client arguments.");
        return 1;
    }

    /* Store settings at user level */
    user->data = settings;

    /* Start game streaming if owner */
    if (user->owner) {

        /* Store owner's settings at client level */
        game_client->settings = settings;

        /* Start game capture/encode thread */
        if (pthread_create(&game_client->game_thread, NULL,
                    guac_game_client_thread, user->client)) {
            guac_user_log(user, GUAC_LOG_ERROR,
                    "Unable to start game streaming thread.");
            return 1;
        }

    }

    /* Only handle events if not read-only */
    if (!settings->read_only) {

        /* Mouse events */
        user->mouse_handler = guac_game_user_mouse_handler;

        /* Keyboard events */
        user->key_handler = guac_game_user_key_handler;

    }

    return 0;

}

int guac_game_user_leave_handler(guac_user* user) {

    guac_game_client* game_client = (guac_game_client*) user->client->data;

    /* Notify display of user leaving */
    if (game_client->display)
        guac_display_notify_user_left(game_client->display, user);

    /* Free settings if not owner (owner settings will be freed with client) */
    if (!user->owner) {
        guac_game_settings* settings = (guac_game_settings*) user->data;
        guac_game_settings_free(settings);
    }

    return 0;

}
