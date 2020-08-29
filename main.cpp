/*****************************************************************************\
  Crazy Cow Ink Lettering
  by Eric Johnston
  23 August 2020

  This is a service which allows the user to simply connect a USB keyboard
  to a reMarkable tabet

  MIT License
  Obviously there is no warranty of any kind. By using this software you may
  cause the destruction of your tablet, your computer, and reality as we know
  it, and this will be your responsibility and yours alone.
  I ask you to use this only to do good, and cause mellow happiness in
  yourself and those around you.
  Copyright 2020 Eric Johnston
\*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <vector>
#include "ccow.h"

// Some useful references
// rm awesome: https://github.com/reHackable/awesome-reMarkable
// override example: https://github.com/ddvk/remarkable-touchgestures
// key codes: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/uapi/linux/input-event-codes.h
// evt struct: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/uapi/linux/input.h
// evt codes: https://www.kernel.org/doc/Documentation/input/event-codes.txt
// fonts: http://www.imajeenyus.com/computer/20150110_single_line_fonts/index.shtml
// Hershey text: https://wiki.evilmadscientist.com/Hershey_Text
// Raw Hershey font files: https://emergent.unpythonic.net/software/hershey

////////////////////////////////////////////////////////////////////////////
// Work in progress, not yet ready to be used.
// So far this is just a proof of concept.
////////////////////////////////////////////////////////////////////////////

static int keyboard = -1;
static int wacom = -1;
static int line_height = 800;
static int font_scale = 30;
static int cursor_x = limit_left;
static int cursor_y = limit_top;
static int left_shift = 0;
static int right_shift = 0;
static int left_ctrl = 0;
static int right_ctrl = 0;
static int left_alt = 0;
static int right_alt = 0;
static int caps_lock = 0;
// static float stylus_x = 0;
// static float stylus_y = 0;

std::vector<struct input_event> pending_events;

void finish_wacom_events()
{
    if (pending_events.size())
    {
        write(wacom, &pending_events[0], pending_events.size() * sizeof(pending_events[0]));
        fsync(wacom);
        pending_events.clear();
    }
}

void send_wacom_event(int type, int code, int value)
{
    struct input_event evt;
    gettimeofday(&evt.time, NULL);
    evt.type = type;
    evt.code = code;
    evt.value = value;
    pending_events.push_back(evt);
    finish_wacom_events();
}

void wacom_char(char ascii_value)
{
    int num_strokes = 0;
    int char_width = 0;
    const int8_t* stroke_data = get_font_char("hershey_font_simplex", ascii_value, num_strokes, char_width);
    if (!stroke_data)
        return;

    const int8_t test_star[] = {
//        10,10, 10-1,10, 10,10-1, 10+1,10, 10,10+1, 10-1,10, -1,-1,

        10,10, 10-10,10   , -1,-1,
        10,10, 10+10,10   , -1,-1,
        10,10, 10   ,10-10, -1,-1,
        10,10, 10   ,10+10, -1,-1,

        10,10, 10-10,10-10, -1,-1,
        10,10, 10+10,10+10, -1,-1,
        10,10, 10+10,10-10, -1,-1,
        10,10, 10-10,10+10, -1,-1,

        10,10, 10-5,10-10, -1,-1,
        10,10, 10+5,10+10, -1,-1,
        10,10, 10+5,10-10, -1,-1,
        10,10, 10-5,10+10, -1,-1,

        10,10, 10-10,10-5, -1,-1,
        10,10, 10+10,10+5, -1,-1,
        10,10, 10+10,10-5, -1,-1,
        10,10, 10-10,10+5, -1,-1,
    };
    stroke_data = test_star;
    num_strokes = (sizeof(test_star) / sizeof(test_star[0])) / 2;
    char_width = 35;

    printf("Ascii %d ('%c'): %d strokes, %d width : ", ascii_value, ascii_value, num_strokes, char_width);
//    int strokes[] = {0, 0, 100, 100, 200, 0, -1000, -1000, 50, 50, 150, 50, -2000, -2000};

    if (num_strokes > 0)
    {
        send_wacom_event(EV_KEY, BTN_TOOL_PEN, 1);
send_wacom_event(0, 0, 0);
finish_wacom_events();
usleep(250 * 1000);
        bool pen_down = false;
        float x = 0;
        float y = 0;
        for (int8_t stroke_index = 0; stroke_index < num_strokes; ++stroke_index)
        {
            int dx = *stroke_data++;
            int dy = *stroke_data++;
            printf("%d,%d   ", dx, dy);
            if (dx == -1 && dy == -1)
            {
                if (pen_down)
                {
//                    pos_defilter(x, y);
                    send_wacom_event(EV_ABS, ABS_X, (int)y);
                    send_wacom_event(EV_ABS, ABS_Y, (int)x);
                    send_wacom_event(EV_KEY, BTN_TOUCH, 0);
                    send_wacom_event(EV_ABS, ABS_DISTANCE, 80);
                    send_wacom_event(0, 0, 0);
                    finish_wacom_events();
finish_wacom_events();
usleep(250 * 1000);
                    pen_down = false;
                }
            }
            else
            {
                x = (float)dx * font_scale + cursor_x;
                y = (float)dy * font_scale + cursor_y;
                for (int iter = 0; iter < 2; ++iter)
                {
                    send_wacom_event(EV_ABS, ABS_X, (int)y);
                    send_wacom_event(EV_ABS, ABS_Y, (int)x);
                    send_wacom_event(EV_ABS, ABS_PRESSURE, 3288);
                    send_wacom_event(EV_ABS, ABS_DISTANCE, 8);
                    send_wacom_event(EV_ABS, ABS_TILT_X, -2600);
                    send_wacom_event(EV_ABS, ABS_TILT_Y, 3500);
                    send_wacom_event(0, 0, 0);
                    finish_wacom_events();
                    if (!pen_down && iter == 0)
                    {
                        send_wacom_event(EV_KEY, BTN_TOUCH, 1);
                        send_wacom_event(0, 0, 0);
                        finish_wacom_events();
finish_wacom_events();
usleep(250 * 1000);
                        pen_down = true;
                    }
                }
            }
        }
        printf("\n");
        if (pen_down)
        {
            send_wacom_event(EV_ABS, ABS_X, (int)y);
            send_wacom_event(EV_ABS, ABS_Y, (int)x);
            send_wacom_event(EV_KEY, BTN_TOUCH, 0);
            send_wacom_event(EV_ABS, ABS_DISTANCE, 80);
finish_wacom_events();
usleep(250 * 1000);
        }
        send_wacom_event(EV_KEY, BTN_TOOL_PEN, 0);
        send_wacom_event(0, 0, 0);
        finish_wacom_events();
finish_wacom_events();
usleep(250 * 1000);
    }
    cursor_x += font_scale * char_width;
    if (cursor_x > limit_right)
    {
        cursor_x = limit_left;
        if (cursor_y > limit_bottom)
            cursor_y -= line_height;
    }
}

void handle_event(const struct input_event* evt)
{
    if (evt->type == EV_KEY)
    {
        switch (evt->code)
        {
        case KEY_LEFTSHIFT:  left_shift  = evt->value; break;
        case KEY_RIGHTSHIFT: right_shift = evt->value; break;
        case KEY_LEFTCTRL:   left_ctrl   = evt->value; break;
        case KEY_RIGHTCTRL:  right_ctrl  = evt->value; break;
        case KEY_LEFTALT:    left_alt    = evt->value; break;
        case KEY_RIGHTALT:   right_alt   = evt->value; break;
        case KEY_CAPSLOCK:
            if (!evt->value)
                caps_lock = !caps_lock;
            break;
        case KEY_ENTER:
            if (evt->value) // key down
            {
                cursor_x = limit_left;
                if (cursor_y > limit_bottom)
                    cursor_y -= line_height;
            }
            break;
        default:
            if (evt->value) // key down
            {
                printf("key, sh=%d\n", (int)(left_shift | right_shift));
                int modifiers = 0;
                if (left_shift | right_shift | caps_lock)
                    modifiers |= MOD_CAPS;
                if (left_shift | right_shift)
                    modifiers |= MOD_SHIFT;
                if (left_ctrl | right_ctrl)
                    modifiers |= MOD_CTRL;
                if (left_alt | right_alt)
                    modifiers |= MOD_ALT;
                char ascii = keycode_to_ascii(evt->code, modifiers);
                if (ascii)
                    wacom_char(ascii);
            }
            break;
        }
    }
    else
    {
//        printf("    %d, %d, %d,\n", (int)evt->type, (int)evt->code, (int)evt->value);
    }
}

void handle_keys()
{
    struct input_event evt;

    keyboard = open(EXTERNAL_USB_KEYBOARD, O_RDONLY);
    wacom = open(WACOM, O_WRONLY);
    if (!keyboard)
    {
        fprintf(stderr, "cannot open keyboard");
        exit(1);
    }
    if (!wacom)
    {
        fprintf(stderr, "cannot open wacom");
        exit(1);
    }
    while(read(keyboard,&evt, sizeof(evt)))
    {
        handle_event(&evt);
    }
    close(keyboard);
}

//@@@@@ ej the BUTTONS device is the buttons, but not the physical keyboard!
int main()
{
    printf("howdy!\n");
    handle_keys();

    return 0;
}
