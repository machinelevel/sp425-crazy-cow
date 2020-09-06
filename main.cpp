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
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <vector>
#include <string>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
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

static int device_keyboard = -1;
static int device_wacom = -1;

// TUNING VARIABLES
static const float default_font_scale = 20;
static const float min_font_scale = 11;
static const float max_font_scale = 75;
static float font_scale = default_font_scale; // Default text size

static int line_height = 35; // This will be scaled by the text size
static float sleep_after_pen_up_ms = 5;
static float sleep_after_pen_down_ms = 5;    // Without this, segments go missing
static float sleep_each_stroke_point_ms = 1;
static float mini_segment_length = 10.0;      // How far to move the pen while drawing

static int cursor_x = limit_left;
static int cursor_y = limit_top;
static int left_shift = 0;
static int right_shift = 0;
static int left_ctrl = 0;
static int right_ctrl = 0;
static int left_alt = 0;
static int right_alt = 0;
static int caps_lock = 0;
static const char* current_font = "hershey";

std::vector<struct input_event> pending_events;

#define BACKSPACE_HIST_LENGTH 1000
int backspace_hist_pos_x[BACKSPACE_HIST_LENGTH];
int backspace_hist_pos_y[BACKSPACE_HIST_LENGTH];
char backspace_hist_char[BACKSPACE_HIST_LENGTH];
int backspace_slot = 0;

static void finish_wacom_events()
{
    if (pending_events.size())
    {
        write(device_wacom, &pending_events[0], pending_events.size() * sizeof(pending_events[0]));
        fsync(device_wacom);
        pending_events.clear();
    }
}

static void send_wacom_event(int type, int code, int value)
{
    struct input_event evt;
    gettimeofday(&evt.time, NULL);
    evt.type = type;
    evt.code = code;
    evt.value = value;
    pending_events.push_back(evt);
    finish_wacom_events();
}

static void condition_strokes_interp(const int8_t* stroke_data, int num_strokes, std::vector<float>& fstrokes)
{
    // This one actually works~
    float last_raw_x = -1;
    float last_raw_y = -1;

    // Stroke the character by scaling
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
                float length = font_scale * sqrt(desired_dx * desired_dx + desired_dy * desired_dy);
                if (length > 0.0001)
                {
                    int subsegments = length / mini_segment_length - 1;
                    for (int i = 0; i < subsegments; ++i)
                    {
                        float t = (float)i / (float)subsegments;
                        float dx = t * desired_dx;
                        float dy = t * desired_dy;
                        float mod_x = last_raw_x + dx;
                        float mod_y = last_raw_y + dy;
                        fstrokes.push_back(mod_x);
                        fstrokes.push_back(mod_y);
                    }
                }
            }
            fstrokes.push_back(raw_x);
            fstrokes.push_back(raw_y);
            last_raw_x = raw_x;
            last_raw_y = raw_y;
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

static void press_ui_button(int x, int y)
{
    // Pen down
    send_wacom_event(EV_KEY, BTN_TOOL_PEN, 1);
    send_wacom_event(EV_KEY, BTN_TOUCH, 0);
    send_wacom_event(EV_ABS, ABS_PRESSURE, 0);
    send_wacom_event(EV_ABS, ABS_DISTANCE, 80);
    send_wacom_event(0, 0, 0);
    finish_wacom_events();
    send_wacom_event(EV_ABS, ABS_X, y);
    send_wacom_event(EV_ABS, ABS_Y, x);
    send_wacom_event(EV_ABS, ABS_PRESSURE, 3288);
    send_wacom_event(EV_ABS, ABS_DISTANCE, 0);
    send_wacom_event(EV_ABS, ABS_TILT_X, 0);
    send_wacom_event(EV_ABS, ABS_TILT_Y, 0);
    send_wacom_event(0, 0, 0);
    finish_wacom_events();
    send_wacom_event(EV_KEY, BTN_TOUCH, 1);
    send_wacom_event(0, 0, 0);
    finish_wacom_events();
    finish_wacom_events();
    usleep(10 * 1000);  // <---- If I remove this, strokes are missing.

    // Pen up
    send_wacom_event(EV_ABS, ABS_X, y);
    send_wacom_event(EV_ABS, ABS_Y, x);
    send_wacom_event(EV_KEY, BTN_TOUCH, 0);
    send_wacom_event(EV_ABS, ABS_DISTANCE, 80);
    finish_wacom_events();
    send_wacom_event(EV_KEY, BTN_TOOL_PEN, 0);
    send_wacom_event(0, 0, 0);
    finish_wacom_events();
}

static void go_to_home_pos()
{
    cursor_x = limit_left;
    cursor_y = limit_top;
}

static void detected_new_page()
{
    go_to_home_pos();
}

static void new_line()
{
    cursor_x = limit_left;
    if (cursor_y > limit_bottom)
        cursor_y -= line_height * font_scale;
    else
        go_to_home_pos();
}

static int get_num_undos(char ascii_value)
{
    int num_strokes = 0;
    int char_width = 0;
    int undo_count = 0;
    const int8_t* stroke_data = get_font_char(current_font, ascii_value, num_strokes, char_width);
    if (num_strokes)
        undo_count = 1;
    for (int stroke_index = 0; stroke_index < num_strokes; ++stroke_index)
    {
        float dx = stroke_data[2 * stroke_index + 0];
        float dy = stroke_data[2 * stroke_index + 1];
        if (dx == -1 && dy == -1)
            undo_count++;
    }
    return undo_count;
}

static void do_backspace()
{
    int prev_slot = (backspace_slot + BACKSPACE_HIST_LENGTH - 1) % BACKSPACE_HIST_LENGTH;
    char ascii_value = backspace_hist_char[prev_slot];
    if (ascii_value)
    {
        int undo_count = get_num_undos(ascii_value);
        for (int i = 0; i < undo_count; ++i)
        {
            // TODO: Put these x,y into a settings file?
            int undo_x = 50;
            int undo_y = 12100;
            press_ui_button(undo_x, undo_y);
            usleep(10 * 1000);
        }
        cursor_x = backspace_hist_pos_x[prev_slot];
        cursor_y = backspace_hist_pos_y[prev_slot];
        backspace_slot = prev_slot;
    }
}

static void wacom_char(char ascii_value, bool wrap_ok);

static void word_wrap()
{
    // Of we flowed off the right side, try to do a word-wrap
    const int max_word_chars = 10;
    int wrap_slot = backspace_slot;
    int bs_count = 0;
    bool done = false;
    while (!done)
    {
        wrap_slot = (wrap_slot + BACKSPACE_HIST_LENGTH - 1) % BACKSPACE_HIST_LENGTH;
        char ascii_value = backspace_hist_char[wrap_slot];
        if (ascii_value == 0 || ascii_value == ' ')
        {
            // We can wrap this
            for (int bs = 0; bs < bs_count; ++bs)
            {
                do_backspace();
                usleep(10 * 1000);
            }
            new_line();
            usleep(500 * 1000); // Allow time for the undo to finish, else we lose strokes.
            for (int bs = 0; bs < bs_count; ++bs)
            {
                wacom_char(backspace_hist_char[backspace_slot], false);
                usleep(10 * 1000);
            }
            done = true;
        }
        else if (bs_count > max_word_chars)
        {
            // Give up and hoit return
            new_line();
            done = true;
        }
        else
        {
            bs_count++;
        }
    }
}

static void backspace_add_char(char ascii_value)
{
    backspace_hist_char[backspace_slot] = ascii_value;
    backspace_hist_pos_x[backspace_slot] = cursor_x;
    backspace_hist_pos_y[backspace_slot] = cursor_y;
    backspace_slot = (backspace_slot + 1) % BACKSPACE_HIST_LENGTH;
}

static void adjust_font_scale(int updown)
{
    if (updown == 1)
    {
        if (font_scale < max_font_scale)
            font_scale *= 1.0 / 0.8;
    }
    else if (updown == -1)
    {
        if (font_scale > min_font_scale)
            font_scale *= 0.8;
    }
    else
        font_scale = default_font_scale;
//    printf("%d: font scale = %f\n", updown, font_scale);
}

static void wacom_char(char ascii_value, bool wrap_ok)
{
    int num_strokes = 0;
    int char_width = 0;

    if (ascii_value)
        backspace_add_char(ascii_value);
    const int8_t* stroke_data = get_font_char(current_font, ascii_value, num_strokes, char_width);
    if (!stroke_data)
        return;

    // Condition the strokes
    static std::vector<float> fstrokes; // static to avoid constant allocation
    fstrokes.clear();
    condition_strokes_interp(stroke_data, num_strokes, fstrokes);
    num_strokes = fstrokes.size() >> 1;

    if (num_strokes > 0)
    {
        send_wacom_event(EV_KEY, BTN_TOOL_PEN, 1);
        send_wacom_event(EV_KEY, BTN_TOUCH, 0);
        send_wacom_event(EV_ABS, ABS_PRESSURE, 0);
        send_wacom_event(EV_ABS, ABS_DISTANCE, 80);
        send_wacom_event(0, 0, 0);
        finish_wacom_events();
        bool pen_down = false;
        float x = 0;
        float y = 0;
        for (int stroke_index = 0; stroke_index < num_strokes; ++stroke_index)
        {
            float dx = fstrokes[2 * stroke_index + 0];
            float dy = fstrokes[2 * stroke_index + 1];
            if (dx == -1 && dy == -1)
            {
                if (pen_down)
                {
                    send_wacom_event(EV_ABS, ABS_X, (int)y);
                    send_wacom_event(EV_ABS, ABS_Y, (int)x);
                    send_wacom_event(EV_KEY, BTN_TOUCH, 0);
                    send_wacom_event(EV_ABS, ABS_PRESSURE, 0);
                    send_wacom_event(EV_ABS, ABS_DISTANCE, 80);
                    send_wacom_event(0, 0, 0);
                    finish_wacom_events();
                    finish_wacom_events();
                    usleep(sleep_after_pen_up_ms * 1000);
                    pen_down = false;
                }
            }
            else
            {
                x = (float)dx * font_scale + cursor_x;
                y = (float)dy * font_scale + cursor_y;
                finish_wacom_events();
                usleep(sleep_each_stroke_point_ms * 1000);
                send_wacom_event(EV_ABS, ABS_X, (int)y);
                send_wacom_event(EV_ABS, ABS_Y, (int)x);
                send_wacom_event(EV_ABS, ABS_PRESSURE, 3288);
                send_wacom_event(EV_ABS, ABS_DISTANCE, 0);
                send_wacom_event(EV_ABS, ABS_TILT_X, 0);
                send_wacom_event(EV_ABS, ABS_TILT_Y, 0);
                send_wacom_event(0, 0, 0);
                finish_wacom_events();
                if (!pen_down)
                {
                    send_wacom_event(EV_KEY, BTN_TOUCH, 1);
                    send_wacom_event(0, 0, 0);
                    finish_wacom_events();
                    usleep(sleep_after_pen_down_ms * 1000);  // <---- If I remove this, strokes are missing.
                    pen_down = true;
                }
            }
        }
        //printf("\n");
        if (pen_down)
        {
            send_wacom_event(EV_ABS, ABS_X, (int)y);
            send_wacom_event(EV_ABS, ABS_Y, (int)x);
            send_wacom_event(EV_KEY, BTN_TOUCH, 0);
            send_wacom_event(EV_ABS, ABS_DISTANCE, 80);
            finish_wacom_events();
        }
        send_wacom_event(EV_KEY, BTN_TOOL_PEN, 0);
        send_wacom_event(0, 0, 0);
        finish_wacom_events();
        finish_wacom_events();
    }
    cursor_x += font_scale * char_width;
    if (cursor_x > limit_right)
    {
        if (wrap_ok)
            word_wrap();
        else
            new_line();
    }
}

static void handle_event(const struct input_event* evt)
{
    if (evt->type == EV_KEY)
    {
        int modifiers = 0;
        if (left_shift | right_shift | caps_lock)
            modifiers |= MOD_CAPS;
        if (left_shift | right_shift)
            modifiers |= MOD_SHIFT;
        if (left_ctrl | right_ctrl)
            modifiers |= MOD_CTRL;
        if (left_alt | right_alt)
            modifiers |= MOD_ALT;

        switch (evt->code)
        {
        case KEY_LEFT:   detected_new_page(); break;
        case KEY_RIGHT:  detected_new_page(); break;
        case KEY_UP:
            if (evt->value)
            {
                cursor_y += line_height * font_scale;
                if (cursor_y > limit_top)
                    cursor_y = limit_top;
            }
            break;
        case KEY_DOWN:
            if (evt->value)
            {
                cursor_y -= line_height * font_scale;
                if (cursor_y < limit_bottom)
                    cursor_y = limit_bottom;
            }
            break;
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
                backspace_add_char(' ');
                new_line();
            }
            break;
        case KEY_BACKSPACE:
            if (evt->value) // key down
                do_backspace();
            break;
        default:
            if (evt->value) // key down
            {
                if (modifiers & MOD_CTRL)
                {
                    if (evt->value == 1)
                    {
                        switch (evt->code)
                        {
                        case KEY_MINUS: adjust_font_scale(-1); break;
                        case KEY_EQUAL: adjust_font_scale(1);  break;
                        case KEY_0:     adjust_font_scale(0);  break;
                        default: break;
                        }
                    }
                }
                else
                {
                    char ascii = keycode_to_ascii(evt->code, modifiers);
                    if (ascii)
                        wacom_char(ascii, true);
                }
            }
            break;
        }
    }
    else
    {
//        printf("    %d, %d, %d,\n", (int)evt->type, (int)evt->code, (int)evt->value);
    }
}

static void find_devices()
{
    std::string path = "/dev/input/by-path/";
    DIR* dirp = opendir(path.c_str());
    struct dirent* dp;
    while ((dp = readdir(dirp)) != NULL)
    {
        if (strstr(dp->d_name, "event-mouse"))
        {
            if (device_wacom == -1)
            {
                device_wacom = open((path + dp->d_name).c_str(), O_WRONLY);
                if (device_wacom >= 0)
                    printf("  Connected to pen input device %s.\n", (path + dp->d_name).c_str());
                else
                    printf("  Failed to connect to pen input device %s.\n", (path + dp->d_name).c_str());
            }
        }
        else if (strstr(dp->d_name, "event-kbd"))
        {
            if (device_keyboard == -1)
            {
//                device_keyboard = open("/dev/input/event2", O_RDONLY);
                device_keyboard = open((path + dp->d_name).c_str(), O_RDONLY);
                if (device_keyboard >= 0)
                    printf("  Connected to keyboard device %s.\n", (path + dp->d_name).c_str());
                else
                    printf("  Failed to connect to keyboard device %s.\n", (path + dp->d_name).c_str());
            }
        }
    }
    closedir(dirp);
}

static void do_main_loop()
{
    struct input_event evt;
    while (1)
    {
        if (device_keyboard < 0)
        {
            find_devices();
            if (device_keyboard < 0)
                usleep(2 * 1000 * 1000);
        }
        else
        {
            int read_result = read(device_keyboard, &evt, sizeof(evt));
            if (read_result > 0)
            {
                handle_event(&evt);
            }
            else
            {
                // Lost the keyboard, lazily try to re-acquire it
                printf("Lost keyboard connection.\n");
                close(device_keyboard);
                device_keyboard = -1;
                usleep(2 * 1000 * 1000);
            }
        }
    }
}

static void initialize()
{
    memset(backspace_hist_char, 0, sizeof(backspace_hist_char));
}

int main()
{
    initialize();
    do_main_loop();
    return 0;
}
