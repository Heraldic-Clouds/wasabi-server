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

#include "capture-x11.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <stdlib.h>
#include <string.h>

/**
 * X11 error handler to check for XShm errors.
 */
static int xshm_error = 0;

static int guac_game_x11_error_handler(Display* display, XErrorEvent* event) {
    xshm_error = 1;
    return 0;
}

int guac_game_x11_capture_init(guac_game_x11_capture* capture,
        const char* display_name) {

    /* Initialize structure */
    memset(capture, 0, sizeof(guac_game_x11_capture));

    /* Open X11 display */
    capture->display = XOpenDisplay(display_name);
    if (capture->display == NULL) {
        return -1;
    }

    /* Get default screen and root window */
    capture->screen = DefaultScreen(capture->display);
    capture->root = RootWindow(capture->display, capture->screen);

    /* Get screen dimensions */
    capture->width = DisplayWidth(capture->display, capture->screen);
    capture->height = DisplayHeight(capture->display, capture->screen);

    /* Check for XShm extension */
    int ignore;
    if (!XShmQueryExtension(capture->display)) {
        /* Fall back to non-shared memory capture */
        capture->use_shm = 0;
        capture->image = NULL;
        return 0;
    }

    /* Try to use XShm */
    capture->use_shm = 1;
    xshm_error = 0;

    /* Install error handler */
    XErrorHandler old_handler = XSetErrorHandler(guac_game_x11_error_handler);

    /* Create XShm image */
    capture->image = XShmCreateImage(
        capture->display,
        DefaultVisual(capture->display, capture->screen),
        DefaultDepth(capture->display, capture->screen),
        ZPixmap,
        NULL,
        &capture->shm_info,
        capture->width,
        capture->height
    );

    if (capture->image == NULL) {
        capture->use_shm = 0;
        XSetErrorHandler(old_handler);
        return 0;
    }

    /* Allocate shared memory segment */
    capture->shm_info.shmid = shmget(
        IPC_PRIVATE,
        capture->image->bytes_per_line * capture->image->height,
        IPC_CREAT | 0777
    );

    if (capture->shm_info.shmid < 0) {
        XDestroyImage(capture->image);
        capture->image = NULL;
        capture->use_shm = 0;
        XSetErrorHandler(old_handler);
        return 0;
    }

    /* Attach shared memory */
    capture->shm_info.shmaddr = capture->image->data = shmat(
        capture->shm_info.shmid, NULL, 0
    );

    if (capture->shm_info.shmaddr == (char*) -1) {
        shmctl(capture->shm_info.shmid, IPC_RMID, NULL);
        XDestroyImage(capture->image);
        capture->image = NULL;
        capture->use_shm = 0;
        XSetErrorHandler(old_handler);
        return 0;
    }

    capture->shm_info.readOnly = False;

    /* Attach to X server */
    if (!XShmAttach(capture->display, &capture->shm_info)) {
        shmdt(capture->shm_info.shmaddr);
        shmctl(capture->shm_info.shmid, IPC_RMID, NULL);
        XDestroyImage(capture->image);
        capture->image = NULL;
        capture->use_shm = 0;
        XSetErrorHandler(old_handler);
        return 0;
    }

    /* Sync to ensure no errors */
    XSync(capture->display, False);

    /* Restore error handler */
    XSetErrorHandler(old_handler);

    /* Check if error occurred */
    if (xshm_error) {
        XShmDetach(capture->display, &capture->shm_info);
        shmdt(capture->shm_info.shmaddr);
        shmctl(capture->shm_info.shmid, IPC_RMID, NULL);
        XDestroyImage(capture->image);
        capture->image = NULL;
        capture->use_shm = 0;
    }

    /* Mark shared memory for deletion when all processes detach */
    shmctl(capture->shm_info.shmid, IPC_RMID, NULL);

    return 0;

}

unsigned char* guac_game_x11_capture_frame(guac_game_x11_capture* capture) {

    if (capture->display == NULL)
        return NULL;

    if (capture->use_shm && capture->image != NULL) {

        /* Capture using XShm (fast path) */
        if (!XShmGetImage(capture->display, capture->root, capture->image,
                    0, 0, AllPlanes)) {
            return NULL;
        }

        return (unsigned char*) capture->image->data;

    }
    else {

        /* Capture using XGetImage (slow fallback) */
        XImage* image = XGetImage(
            capture->display,
            capture->root,
            0, 0,
            capture->width,
            capture->height,
            AllPlanes,
            ZPixmap
        );

        if (image == NULL)
            return NULL;

        /* Store for cleanup */
        if (capture->image != NULL)
            XDestroyImage(capture->image);
        capture->image = image;

        return (unsigned char*) image->data;

    }

}

int guac_game_x11_capture_get_width(guac_game_x11_capture* capture) {
    return capture->width;
}

int guac_game_x11_capture_get_height(guac_game_x11_capture* capture) {
    return capture->height;
}

void guac_game_x11_capture_cleanup(guac_game_x11_capture* capture) {

    if (capture->use_shm && capture->image != NULL) {
        XShmDetach(capture->display, &capture->shm_info);
        shmdt(capture->shm_info.shmaddr);
        /* Note: shmctl IPC_RMID was already called in init */
    }

    if (capture->image != NULL) {
        XDestroyImage(capture->image);
        capture->image = NULL;
    }

    if (capture->display != NULL) {
        XCloseDisplay(capture->display);
        capture->display = NULL;
    }

}
