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
static int line_height = 30;
static int font_scale = 30 * 5;
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

void condition_strokes1(const int8_t* stroke_data, int num_strokes, std::vector<float>& fstrokes)
{
    // The reMarkable strokes problem is that the pen travels too far while down.
    // This version fixes the problem by drawing each segment as two half-length
    // segments, meeting (overshooting) in the middle.
    // It works, but the text recognition is totally thrown off by it.
    int last_dx = -1;
    int last_dy = -1;
    for (int i = 0; i < num_strokes; ++i)
    {
        int dx = stroke_data[2 * i + 0];
        int dy = stroke_data[2 * i + 1];
        if (dx != -1 || dy != -1)
        {
            if (last_dx != -1 || last_dy != -1)
            {
                float mx = 0.5 * dx + 0.5 * last_dx;
                float my = 0.5 * dy + 0.5 * last_dy;
                fstrokes.push_back(last_dx);
                fstrokes.push_back(last_dy);
                fstrokes.push_back(mx);
                fstrokes.push_back(my);
                fstrokes.push_back(-1);
                fstrokes.push_back(-1);
                fstrokes.push_back(dx);
                fstrokes.push_back(dy);
                fstrokes.push_back(mx);
                fstrokes.push_back(my);
                fstrokes.push_back(-1);
                fstrokes.push_back(-1);
            }
            last_dx = dx;
            last_dy = dy;
        }
        else
        {
            last_dx = -1;
            last_dy = -1;
        }
    }    
}

void condition_strokes2(const int8_t* stroke_data, int num_strokes, std::vector<float>& fstrokes)
{
    // The reMarkable strokes problem is that the pen travels too far while down.
    // This version fixes the problem by scaling down the distance traveled in each
    // segment by a constant factor.
    float scale_down = 2.0 / 3.0;
    float last_raw_x = -1;
    float last_raw_y = -1;
    float last_mod_x = -1;
    float last_mod_y = -1;
    for (int i = 0; i < num_strokes; ++i)
    {
        float raw_x = stroke_data[2 * i + 0];
        float raw_y = stroke_data[2 * i + 1];
        if (raw_x != -1 || raw_y != -1)
        {
            float mod_x;
            float mod_y;
            if (last_raw_x != -1 || last_raw_y != -1)
            {
                float desired_dx = raw_x - last_raw_x;
                float desired_dy = raw_y - last_raw_y;
                mod_x = last_mod_x + scale_down * desired_dx;
                mod_y = last_mod_y + scale_down * desired_dy;
            }
            else
            {
                mod_x = raw_x;
                mod_y = raw_y;
            }
            fstrokes.push_back(mod_x);
            fstrokes.push_back(mod_y);
            last_raw_x = raw_x;
            last_raw_y = raw_y;
            last_mod_x = mod_x;
            last_mod_y = mod_y;
        }
        else
        {
            last_raw_x = -1;
            last_raw_y = -1;
            fstrokes.push_back(-1);
            fstrokes.push_back(-1);
        }
    }    
}

void condition_strokes3(const int8_t* stroke_data, int num_strokes, std::vector<float>& fstrokes)
{
    // The reMarkable strokes problem is that the pen travels too far while down.
    // This version fixes the problem by scaling down the distance traveled in each
    // segment by a constant factor.
    float scale_down = 2.0 / 3.0;
    float last_raw_x = -1;
    float last_raw_y = -1;
    for (int i = 0; i < num_strokes; ++i)
    {
        float raw_x = stroke_data[2 * i + 0];
        float raw_y = stroke_data[2 * i + 1];
        if (raw_x != -1 || raw_y != -1)
        {
            if (last_raw_x != -1 || last_raw_y != -1)
            {
                float desired_dx = raw_x - last_raw_x;
                float desired_dy = raw_y - last_raw_y;
                float mod_x = last_raw_x + scale_down * desired_dx;
                float mod_y = last_raw_y + scale_down * desired_dy;
                fstrokes.push_back(last_raw_x);
                fstrokes.push_back(last_raw_y);
                fstrokes.push_back(mod_x);
                fstrokes.push_back(mod_y);
            }
        }
        else
        {
            fstrokes.push_back(-1);
            fstrokes.push_back(-1);
        }
        last_raw_x = raw_x;
        last_raw_y = raw_y;
    }    
}

void condition_strokes_debug(const int8_t* stroke_data, int num_strokes, std::vector<float>& fstrokes)
{
    // The reMarkable strokes problem is that the pen travels too far while down.
    // This version fixes the problem by scaling down the distance traveled in each
    // segment by a constant factor.
    float scale_down = 2.0 / 3.0;
    float last_raw_x = -1;
    float last_raw_y = -1;
    float last_mod_x = -1;
    float last_mod_y = -1;

    // Draw dots
    float dot_scale = 0.2;
    for (int i = 0; i < num_strokes; ++i)
    {
        float raw_x = stroke_data[2 * i + 0];
        float raw_y = stroke_data[2 * i + 1];
        if (raw_x != -1 || raw_y != -1)
        {
            fstrokes.push_back(raw_x - dot_scale * (1));
            fstrokes.push_back(raw_y);
            fstrokes.push_back(raw_x + dot_scale * (-1 + 2 * scale_down));
            fstrokes.push_back(raw_y);
            fstrokes.push_back(-1);
            fstrokes.push_back(-1);
            fstrokes.push_back(raw_x);
            fstrokes.push_back(raw_y - dot_scale * (1));
            fstrokes.push_back(raw_x);
            fstrokes.push_back(raw_y + dot_scale * (-1 + 2 * scale_down));
            fstrokes.push_back(-1);
            fstrokes.push_back(-1);
            // fstrokes.push_back(raw_x - dot_scale * (1));
            // fstrokes.push_back(raw_y - dot_scale * (1));
            // fstrokes.push_back(raw_x - dot_scale * (1 + 2 * scale_down));
            // fstrokes.push_back(raw_y - dot_scale * (1 + 2 * scale_down));
            // fstrokes.push_back(-1);
            // fstrokes.push_back(-1);
            // fstrokes.push_back(raw_x - dot_scale * (1));
            // fstrokes.push_back(raw_y + dot_scale * (1));
            // fstrokes.push_back(raw_x - dot_scale * (1 + 2 * scale_down));
            // fstrokes.push_back(raw_y + dot_scale * (1 - 2 * scale_down));
            // fstrokes.push_back(-1);
            // fstrokes.push_back(-1);
        }
    }    

    // Stroke the character by scaling
    for (int i = 0; i < num_strokes; ++i)
    {
        float raw_x = stroke_data[2 * i + 0];
        float raw_y = stroke_data[2 * i + 1];
        if (raw_x != -1 || raw_y != -1)
        {
            float mod_x;
            float mod_y;
            if (last_raw_x != -1 || last_raw_y != -1)
            {
                float desired_dx = raw_x - last_raw_x;
                float desired_dy = raw_y - last_raw_y;
                mod_x = last_mod_x + scale_down * desired_dx;
                mod_y = last_mod_y + scale_down * desired_dy;
            }
            else
            {
                mod_x = raw_x;
                mod_y = raw_y;
            }
            fstrokes.push_back(mod_x);
            fstrokes.push_back(mod_y);
            last_raw_x = raw_x;
            last_raw_y = raw_y;
            last_mod_x = mod_x;
            last_mod_y = mod_y;
        }
        else
        {
            last_raw_x = -1;
            last_raw_y = -1;
            fstrokes.push_back(-1);
            fstrokes.push_back(-1);
        }
    }    
}

void wacom_char(char ascii_value)
{
    int num_strokes = 0;
    int char_width = 0;
    const int8_t* stroke_data = get_font_char("hershey_font_simplex", ascii_value, num_strokes, char_width);
    if (!stroke_data)
        return;
#if 0
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
    const int8_t test_x[] = {
        10-5,10-10, 10+5,10+10, -1,-1,
        10+5,10-10, 10-5,10+10, -1,-1,
    };
    const int8_t test_x2[] = { // 4 spokes outside in
        10-5,10-10, 10,10, -1,-1,
        10+5,10-10, 10,10, -1,-1,
        10+5,10+10, 10,10, -1,-1,
        10-5,10+10, 10,10, -1,-1,
    };
    const int8_t test_x3[] = { // 4 spokes inside out
        10,10, 10-5,10-10, -1,-1,
        10,10, 10+5,10-10, -1,-1,
        10,10, 10+5,10+10, -1,-1,
        10,10, 10-5,10+10, -1,-1,
    };

    const int8_t test_x4[] = { // 4 spokes outside in
        10,10, 10-1,10, 10,10-1, 10+1,10, 10,10+1, 10-1,10, -1,-1,
        10+5,10-10, 10+5-1,10-10, 10+5,10-10-1, 10+5+1,10-10, 10+5,10-10+1, 10+5-1,10-10, -1,-1,
//        10-5,10-10, 10,10, -1,-1,
        10+5,10-10, 10,10, -1,-1,
    };
    const int8_t test_x5[] = { // 4 spokes inside out
        10,10, 10-1,10, 10,10-1, 10+1,10, 10,10+1, 10-1,10, -1,-1,
        10+5,10-10, 10+5-1,10-10, 10+5,10-10-1, 10+5+1,10-10, 10+5,10-10+1, 10+5-1,10-10, -1,-1,
//        10,10, 10-5,10-10, -1,-1,
        10,10, 10+5,10-10, -1,-1,
    };
    const int8_t test_x6[] = { // 4 spokes outside in
        10-5,10+10, 10,10, -1,-1,
        10+5,10+10, 10,10, -1,-1,
    };
    const int8_t test_x7[] = { // 4 spokes inside out
        10,10, 10-5,10+10, -1,-1,
        10,10, 10+5,10+10, -1,-1,
    };
    char_width = 35;
    stroke_data = test_star;
    num_strokes = (sizeof(test_star) / sizeof(test_star[0])) / 2;
    if (ascii_value == 'x')
    {
        stroke_data = test_x;
        num_strokes = (sizeof(test_x) / sizeof(test_x[0])) / 2;
    }
    else if (ascii_value == 'c')
    {
        stroke_data = test_x2;
        num_strokes = (sizeof(test_x2) / sizeof(test_x2[0])) / 2;
    }
    else if (ascii_value == 'v')
    {
        stroke_data = test_x3;
        num_strokes = (sizeof(test_x3) / sizeof(test_x3[0])) / 2;
    }
    else if (ascii_value == 'b')
    {
        stroke_data = test_x4;
        num_strokes = (sizeof(test_x4) / sizeof(test_x4[0])) / 2;
    }
    else if (ascii_value == 'n')
    {
        stroke_data = test_x5;
        num_strokes = (sizeof(test_x5) / sizeof(test_x5[0])) / 2;
    }
    else if (ascii_value == 'm')
    {
        stroke_data = test_x6;
        num_strokes = (sizeof(test_x6) / sizeof(test_x6[0])) / 2;
    }
    else if (ascii_value == ',')
    {
        stroke_data = test_x7;
        num_strokes = (sizeof(test_x7) / sizeof(test_x7[0])) / 2;
    }
#endif

    // Condition the strokes
    std::vector<float> fstrokes;
//    condition_strokes1(stroke_data, num_strokes, fstrokes);
    condition_strokes_debug(stroke_data, num_strokes, fstrokes);
    num_strokes = fstrokes.size() >> 1;

    printf("Ascii %d ('%c'): %d strokes, %d width : ", ascii_value, ascii_value, num_strokes, char_width);
//    int strokes[] = {0, 0, 100, 100, 200, 0, -1000, -1000, 50, 50, 150, 50, -2000, -2000};

    if (num_strokes > 0)
    {
        send_wacom_event(EV_KEY, BTN_TOOL_PEN, 1);
send_wacom_event(EV_KEY, BTN_TOUCH, 0);
send_wacom_event(EV_ABS, ABS_PRESSURE, 0);
send_wacom_event(EV_ABS, ABS_DISTANCE, 80);
send_wacom_event(0, 0, 0);
finish_wacom_events();
//usleep(250 * 1000);
        bool pen_down = false;
        float x = 0;
        float y = 0;
        for (int stroke_index = 0; stroke_index < num_strokes; ++stroke_index)
        {
            float dx = fstrokes[2 * stroke_index + 0];
            float dy = fstrokes[2 * stroke_index + 1];
            printf("%f,%f   ", dx, dy);
            if (dx == -1 && dy == -1)
            {
                if (pen_down)
                {
//                    pos_defilter(x, y);
                   send_wacom_event(EV_ABS, ABS_X, (int)y);
                   send_wacom_event(EV_ABS, ABS_Y, (int)x);
                   send_wacom_event(EV_KEY, BTN_TOUCH, 0);
send_wacom_event(EV_ABS, ABS_PRESSURE, 0);
send_wacom_event(EV_ABS, ABS_DISTANCE, 80);
                    send_wacom_event(0, 0, 0);
                    finish_wacom_events();
finish_wacom_events();
//usleep(250 * 1000);
                    pen_down = false;
                }
            }
            else
            {
                x = (float)dx * font_scale + cursor_x;
                y = (float)dy * font_scale + cursor_y;
                for (int iter = 0; iter < 1; ++iter)
                {
finish_wacom_events();
//usleep(250 * 1000);
                    send_wacom_event(EV_ABS, ABS_X, (int)y);
                    send_wacom_event(EV_ABS, ABS_Y, (int)x);
                    send_wacom_event(EV_ABS, ABS_PRESSURE, 3288);
send_wacom_event(EV_ABS, ABS_DISTANCE, 0);
                    send_wacom_event(EV_ABS, ABS_TILT_X, 0);
                    send_wacom_event(EV_ABS, ABS_TILT_Y, 0);
                    send_wacom_event(0, 0, 0);
                    finish_wacom_events();
                    if (!pen_down && iter == 0)
                    {
                        send_wacom_event(EV_KEY, BTN_TOUCH, 1);
                        send_wacom_event(0, 0, 0);
                        finish_wacom_events();
finish_wacom_events();
usleep(1 * 1000); // <---- If I remove this, strokes are missing.
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
//usleep(250 * 1000);
        }
        send_wacom_event(EV_KEY, BTN_TOOL_PEN, 0);
        send_wacom_event(0, 0, 0);
        finish_wacom_events();
finish_wacom_events();
//usleep(250 * 1000);
    }
    cursor_x += font_scale * char_width;
    if (cursor_x > limit_right)
    {
        cursor_x = limit_left;
        if (cursor_y > limit_bottom)
            cursor_y -= line_height * font_scale;
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
                    cursor_y -= line_height * font_scale;
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
