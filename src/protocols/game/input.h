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

#ifndef GUAC_GAME_INPUT_H
#define GUAC_GAME_INPUT_H

#include <guacamole/user.h>

#include <X11/Xlib.h>

/**
 * Input injection context for sending keyboard/mouse events to X11.
 */
typedef struct guac_game_input {

    /**
     * X11 display connection for input injection.
     */
    Display* display;

    /**
     * Current mouse button mask.
     */
    int button_mask;

    /**
     * Current mouse X position.
     */
    int mouse_x;

    /**
     * Current mouse Y position.
     */
    int mouse_y;

} guac_game_input;

/**
 * Initializes the input injection system.
 *
 * @param input
 *     The input context to initialize.
 *
 * @param display
 *     The X11 display to use for input injection.
 *
 * @return
 *     Zero on success, non-zero on failure.
 */
int guac_game_input_init(guac_game_input* input, Display* display);

/**
 * Injects a mouse event into the X11 display.
 *
 * @param input
 *     The input context.
 *
 * @param x
 *     The mouse X coordinate.
 *
 * @param y
 *     The mouse Y coordinate.
 *
 * @param button_mask
 *     The Guacamole button mask.
 *
 * @return
 *     Zero on success, non-zero on failure.
 */
int guac_game_input_mouse(guac_game_input* input, int x, int y, int button_mask);

/**
 * Injects a keyboard event into the X11 display.
 *
 * @param input
 *     The input context.
 *
 * @param keysym
 *     The X11 keysym for the key.
 *
 * @param pressed
 *     Non-zero if key is pressed, zero if released.
 *
 * @return
 *     Zero on success, non-zero on failure.
 */
int guac_game_input_key(guac_game_input* input, int keysym, int pressed);

/**
 * Cleans up the input injection system.
 *
 * @param input
 *     The input context to clean up.
 */
void guac_game_input_cleanup(guac_game_input* input);

/**
 * Handler for mouse events from Guacamole users.
 */
guac_user_mouse_handler guac_game_user_mouse_handler;

/**
 * Handler for keyboard events from Guacamole users.
 */
guac_user_key_handler guac_game_user_key_handler;

#endif /* GUAC_GAME_INPUT_H */
