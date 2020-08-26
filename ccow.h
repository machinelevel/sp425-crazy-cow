

#ifndef _CCOW_H_
#define _CCOW_H_

#include <stdint.h>

#define WACOM                 "/dev/input/event0"
#define EXTERNAL_USB_KEYBOARD "/dev/input/event3"

#define limit_left    1800
#define limit_right  14500
#define limit_top    19000
#define limit_bottom  1200

const int8_t* get_font_char(const char* font_name, char ascii_value, int& out_num_verts, int& out_horiz_dist);
char keycode_to_ascii(int keycode, bool shifted);

#endif // _CCOW_H_
