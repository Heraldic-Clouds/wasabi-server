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

#ifndef GUAC_GAME_H
#define GUAC_GAME_H

#include <guacamole/client.h>

/**
 * Main game streaming thread function. Captures the screen, encodes video
 * frames using NVENC, and sends them to connected clients.
 *
 * @param data
 *     The guac_client associated with this game connection.
 *
 * @return
 *     Always NULL.
 */
void* guac_game_client_thread(void* data);

/**
 * Initializes the game streaming pipeline including X11 capture and
 * NVENC encoding.
 *
 * @param client
 *     The guac_client to initialize.
 *
 * @return
 *     Zero on success, non-zero on failure.
 */
int guac_game_init_pipeline(guac_client* client);

/**
 * Cleans up the game streaming pipeline.
 *
 * @param client
 *     The guac_client to clean up.
 */
void guac_game_cleanup_pipeline(guac_client* client);

#endif /* GUAC_GAME_H */
