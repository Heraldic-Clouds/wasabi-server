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
#include "settings.h"
#include "user.h"

#include <guacamole/client.h>
#include <guacamole/display.h>
#include <guacamole/mem.h>

#include <pthread.h>
#include <stdlib.h>

/**
 * A pending join handler implementation that will synchronize the connection
 * state for all pending users prior to them being promoted to full user.
 *
 * @param client
 *     The client whose pending users are about to be promoted.
 *
 * @return
 *     Always zero.
 */
static int guac_game_join_pending_handler(guac_client* client) {

    guac_game_client* game_client = (guac_game_client*) client->data;
    guac_socket* broadcast_socket = client->pending_socket;

    /* Synchronize with current display */
    if (game_client->display != NULL) {
        guac_display_dup(game_client->display, broadcast_socket);
        guac_socket_flush(broadcast_socket);
    }

    return 0;

}

int guac_client_init(guac_client* client) {

    /* Set client args */
    client->args = GUAC_GAME_CLIENT_ARGS;

    /* Allocate client data */
    guac_game_client* game_client = guac_mem_zalloc(sizeof(guac_game_client));
    client->data = game_client;

    /* Initialize mutex */
    pthread_mutex_init(&game_client->lock, NULL);

    /* Set handlers */
    client->join_handler = guac_game_user_join_handler;
    client->join_pending_handler = guac_game_join_pending_handler;
    client->leave_handler = guac_game_user_leave_handler;
    client->free_handler = guac_game_client_free_handler;

    return 0;

}

int guac_game_client_free_handler(guac_client* client) {

    guac_game_client* game_client = (guac_game_client*) client->data;

    /* Stop the game thread if running */
    game_client->running = 0;

    /* Ensure all background rendering processes are stopped before freeing
     * underlying memory */
    if (game_client->display != NULL)
        guac_display_stop(game_client->display);

    /* Wait for game thread to finish */
    pthread_join(game_client->game_thread, NULL);

    /* Clean up the streaming pipeline */
    guac_game_cleanup_pipeline(client);

    /* Free display */
    if (game_client->display != NULL)
        guac_display_free(game_client->display);

    /* Free settings */
    if (game_client->settings != NULL)
        guac_game_settings_free(game_client->settings);

    /* Destroy mutex */
    pthread_mutex_destroy(&game_client->lock);

    /* Free client data */
    guac_mem_free(game_client);

    return 0;

}
