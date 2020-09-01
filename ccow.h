

#ifndef _CCOW_H_
#define _CCOW_H_

#include <stdint.h>

#define MOD_CAPS  0x0001
#define MOD_SHIFT 0x0002
#define MOD_CTRL  0x0004
#define MOD_ALT   0x0008

#define limit_left    1800
#define limit_right  14500
#define limit_top    19000
#define limit_bottom  1200

const int8_t* get_font_char(const char* font_name, char ascii_value, int& out_num_verts, int& out_horiz_dist);
char keycode_to_ascii(int keycode, int mods);

#endif // _CCOW_H_
