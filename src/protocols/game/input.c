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
#include "input.h"

#include <guacamole/client.h>
#include <guacamole/user.h>

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#include <pthread.h>
#include <stdlib.h>

/* Guacamole mouse button masks */
#define GUAC_MOUSE_LEFT       0x01
#define GUAC_MOUSE_MIDDLE     0x02
#define GUAC_MOUSE_RIGHT      0x04
#define GUAC_MOUSE_SCROLL_UP  0x08
#define GUAC_MOUSE_SCROLL_DOWN 0x10

int guac_game_input_init(guac_game_input* input, Display* display) {

    input->display = display;
    input->button_mask = 0;
    input->mouse_x = 0;
    input->mouse_y = 0;

    /* Check for XTest extension */
    int event_base, error_base;
    int major_version, minor_version;

    if (!XTestQueryExtension(display, &event_base, &error_base,
                &major_version, &minor_version)) {
        return -1;
    }

    return 0;

}

int guac_game_input_mouse(guac_game_input* input, int x, int y, int button_mask) {

    if (input->display == NULL)
        return -1;

    /* Move mouse if position changed */
    if (x != input->mouse_x || y != input->mouse_y) {
        XTestFakeMotionEvent(input->display, -1, x, y, CurrentTime);
        input->mouse_x = x;
        input->mouse_y = y;
    }

    /* Handle button changes */
    int changed = input->button_mask ^ button_mask;

    /* Left button */
    if (changed & GUAC_MOUSE_LEFT) {
        XTestFakeButtonEvent(input->display, 1,
                (button_mask & GUAC_MOUSE_LEFT) != 0, CurrentTime);
    }

    /* Middle button */
    if (changed & GUAC_MOUSE_MIDDLE) {
        XTestFakeButtonEvent(input->display, 2,
                (button_mask & GUAC_MOUSE_MIDDLE) != 0, CurrentTime);
    }

    /* Right button */
    if (changed & GUAC_MOUSE_RIGHT) {
        XTestFakeButtonEvent(input->display, 3,
                (button_mask & GUAC_MOUSE_RIGHT) != 0, CurrentTime);
    }

    /* Scroll up */
    if (changed & GUAC_MOUSE_SCROLL_UP) {
        if (button_mask & GUAC_MOUSE_SCROLL_UP) {
            XTestFakeButtonEvent(input->display, 4, True, CurrentTime);
            XTestFakeButtonEvent(input->display, 4, False, CurrentTime);
        }
    }

    /* Scroll down */
    if (changed & GUAC_MOUSE_SCROLL_DOWN) {
        if (button_mask & GUAC_MOUSE_SCROLL_DOWN) {
            XTestFakeButtonEvent(input->display, 5, True, CurrentTime);
            XTestFakeButtonEvent(input->display, 5, False, CurrentTime);
        }
    }

    input->button_mask = button_mask;

    /* Flush to ensure events are sent */
    XFlush(input->display);

    return 0;

}

int guac_game_input_key(guac_game_input* input, int keysym, int pressed) {

    if (input->display == NULL)
        return -1;

    /* Convert keysym to keycode */
    KeyCode keycode = XKeysymToKeycode(input->display, keysym);
    if (keycode == 0)
        return -1;

    /* Send key event */
    XTestFakeKeyEvent(input->display, keycode, pressed, CurrentTime);
    XFlush(input->display);

    return 0;

}

void guac_game_input_cleanup(guac_game_input* input) {
    /* Nothing to clean up - display is owned by capture context */
    input->display = NULL;
}

int guac_game_user_mouse_handler(guac_user* user, int x, int y, int button_mask) {

    guac_client* client = user->client;
    guac_game_client* game_client = (guac_game_client*) client->data;

    /* Ensure we have an X11 display for input */
    if (game_client->x11_display == NULL)
        return 0;

    /* Create input context if needed (we share the display) */
    guac_game_input input;
    if (guac_game_input_init(&input, (Display*) game_client->x11_display) != 0)
        return 0;

    /* Lock for thread safety */
    pthread_mutex_lock(&game_client->lock);

    /* Inject mouse event */
    guac_game_input_mouse(&input, x, y, button_mask);

    pthread_mutex_unlock(&game_client->lock);

    return 0;

}

int guac_game_user_key_handler(guac_user* user, int keysym, int pressed) {

    guac_client* client = user->client;
    guac_game_client* game_client = (guac_game_client*) client->data;

    /* Ensure we have an X11 display for input */
    if (game_client->x11_display == NULL)
        return 0;

    /* Create input context if needed */
    guac_game_input input;
    if (guac_game_input_init(&input, (Display*) game_client->x11_display) != 0)
        return 0;

    /* Lock for thread safety */
    pthread_mutex_lock(&game_client->lock);

    /* Inject key event */
    guac_game_input_key(&input, keysym, pressed);

    pthread_mutex_unlock(&game_client->lock);

    return 0;

}
