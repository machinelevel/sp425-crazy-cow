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
#include "ccow.h"

// Some useful references
// rm awesome: https://github.com/reHackable/awesome-reMarkable
// override example: https://github.com/ddvk/remarkable-touchgestures
// key codes: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/uapi/linux/input-event-codes.h
// evt struct: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/uapi/linux/input.h
// evt codes: https://www.kernel.org/doc/Documentation/input/event-codes.txt
// fonts: http://www.imajeenyus.com/computer/20150110_single_line_fonts/index.shtml
// hershey text: https://wiki.evilmadscientist.com/Hershey_Text

////////////////////////////////////////////////////////////////////////////
// Work in progress, not yet ready to be used.
// So far this is just a proof of concept.
////////////////////////////////////////////////////////////////////////////

static int keyboard = -1;
static int wacom = -1;
static int line_height = 800;
static int cursor_x = limit_left;
static int cursor_y = limit_top;

void send_wacom_event(int type, int code, int value)
{
    struct input_event evt;
    gettimeofday(&evt.time, NULL);
    evt.type = type;
    evt.code = code;
    evt.value = value;
    write(wacom, &evt, sizeof(evt));
}

void wacom_char()
{
    int strokes[] = {0, 0, 100, 100, 200, 0, -1000, -1000, 50, 50, 150, 50, -2000, -2000};
    send_wacom_event(EV_KEY, BTN_TOOL_PEN, 1);
    bool done = false;
    bool pen_down = false;
    const int* seg = strokes;
    int x_max = 0;
    while (!done)
    {
        int x = *seg++;
        int y = *seg++;
        if (x == -2000 && y == -2000)
        {
            done = true;
        }
        else if (x == -1000 && y == -1000)
        {
            // if (pen_down)
            // {
            //     send_wacom_event(EV_KEY, BTN_TOUCH, 0);
            //     send_wacom_event(EV_ABS, ABS_DISTANCE, 80);
            //     send_wacom_event(0, 0, 0);
            //     pen_down = false;
            // }
        }
        else
        {
            x *= 4;
            y *= 4;
            if (x_max < x)
                x_max = x;
            send_wacom_event(EV_ABS, ABS_X, cursor_y + y);
            send_wacom_event(EV_ABS, ABS_Y, cursor_x + x);
            send_wacom_event(0, 0, 0);
            if (!pen_down)
            {
                send_wacom_event(EV_KEY, BTN_TOUCH, 1);
                send_wacom_event(EV_ABS, ABS_PRESSURE, 3288);
                send_wacom_event(EV_ABS, ABS_DISTANCE, 8);
                send_wacom_event(EV_ABS, ABS_TILT_X, -2600);
                send_wacom_event(EV_ABS, ABS_TILT_Y, 3500);
                send_wacom_event(0, 0, 0);
                send_wacom_event(EV_ABS, ABS_X, cursor_y + y);
                send_wacom_event(EV_ABS, ABS_Y, cursor_x + x);
                send_wacom_event(0, 0, 0);
                pen_down = true;
            }
        }
    }
    if (pen_down)
    {
        send_wacom_event(EV_KEY, BTN_TOUCH, 0);
        send_wacom_event(EV_ABS, ABS_DISTANCE, 80);
    }
    send_wacom_event(EV_KEY, BTN_TOOL_PEN, 0);
    send_wacom_event(0, 0, 0);
    if (x_max > 0)
    {
        cursor_x += x_max + 100;
        if (cursor_x > limit_right)
        {
            cursor_x = limit_left;
            if (cursor_y > limit_bottom)
                cursor_y -= line_height;
        }
        printf("x: %d y: %d\n", cursor_x, cursor_y);
    }
}

void handle_event(const struct input_event* evt)
{
    if (evt->type == EV_KEY)
    {
        if (evt->value) // it's a key-down, not a key-up
        {
            if (evt->code == KEY_A)
                wacom_char();
            else if (evt->code == KEY_ENTER)
            {
                cursor_x = limit_left;
                if (cursor_y > limit_bottom)
                    cursor_y -= line_height;
            }
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
    wacom = open(WACOM, O_RDWR);
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
