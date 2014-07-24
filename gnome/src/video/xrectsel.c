/*
 * This code is based and adapted from:
 * https://github.com/lolilolicon/FFcast2/blob/master/xrectsel.c
 */

#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
xrectsel(unsigned *x_sel, unsigned *y_sel, unsigned *w_sel, unsigned *h_sel)
{
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy)
        return;

    Window root = DefaultRootWindow(dpy);

    XEvent ev;

    GC sel_gc;
    XGCValues sel_gv;

    int btn_pressed = 0;
    int x = 0, y = 0;
    unsigned int width = 0, height = 0;
    int start_x = 0, start_y = 0;

    Cursor cursor;
    cursor = XCreateFontCursor(dpy, XC_crosshair);

    /* Grab pointer for these events */
    XGrabPointer(dpy, root, True, PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
            GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime);

    sel_gv.function = GXinvert;
    sel_gv.subwindow_mode = IncludeInferiors;
    sel_gv.line_width = 1;
    sel_gc = XCreateGC(dpy, root, GCFunction | GCSubwindowMode | GCLineWidth, &sel_gv);

    for (;;) {
        XNextEvent(dpy, &ev);

        if (ev.type == ButtonPress) {
            btn_pressed = 1;
            x = start_x = ev.xbutton.x_root;
            y = start_y = ev.xbutton.y_root;
            width = height = 0;

        } else if (ev.type == MotionNotify) {
            if (!btn_pressed)
                continue; /* Draw only if button is pressed */

            /* Re-draw last Rectangle to clear it */
            XDrawRectangle(dpy, root, sel_gc, x, y, width, height);

            x = ev.xbutton.x_root;
            y = ev.xbutton.y_root;

            if (x > start_x) {
                width = x - start_x;
                x = start_x;
            } else {
                width = start_x - x;
            }

            if (y > start_y) {
                height = y - start_y;
                y = start_y;
            } else {
                height = start_y - y;
            }

            /* Draw Rectangle */
            XDrawRectangle(dpy, root, sel_gc, x, y, width, height);
            XFlush(dpy);

        } else if (ev.type == ButtonRelease)
            break;
    }

    /* Re-draw last Rectangle to clear it */
    XDrawRectangle(dpy, root, sel_gc, x, y, width, height);
    XFlush(dpy);

    XUngrabPointer(dpy, CurrentTime);
    XFreeCursor(dpy, cursor);
    XFreeGC(dpy, sel_gc);
    XSync(dpy, 1);

    *x_sel = x;
    *y_sel = y;
    *w_sel = width;
    *h_sel = height;

    XCloseDisplay(dpy);
}
