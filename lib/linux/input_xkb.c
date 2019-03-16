#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "halcyonix.h"

#define countof(x) (sizeof(x)/sizeof(*(x)))

static const struct __attribute__((packed)) xkb_km {
	char name[XkbKeyNameLength];
	uint8_t hc_key;
} xkb_map_special[] = {

	// keys with ascii / escape code representation
	{ "ESC" , '\e' }, { "TAB" , '\t' }, { "BKSP", '\b' }, { "RTRN", '\n' },
	{ "LSGT", '\\' }, { "TLDE", '~'  }, { "BKSL", '#'  }, { "SPCE", ' '  },

	// Control (modifier) keys -> 0xCX
	{ "CAPS", 0xC0 },
	{ "LFSH", 0xC1 }, { "LCTL", 0xC2 }, { "LWIN", 0xC3 }, { "LALT", 0xC4 },
	{ "RTSH", 0xC5 }, { "RCTL", 0xC6 }, { "RWIN", 0xC7 }, { "RALT", 0xC8 },
	{ "MENU", 0xC9 },

	// Arrow keys -> 0xAX
	{ "UP"  , 0xA0 }, { "RGHT", 0xA1 }, { "DOWN", 0xA2 }, { "LEFT", 0xA3 },

	// Extra keys -> 0xEX
	{ "PRSC", 0xE0 }, { "SCLK", 0xE1 }, { "PAUS", 0xE2 },
	{ "INS" , 0xE3 }, { "HOME", 0xE4 }, { "PGUP", 0xE5 },
	{ "DELE", 0xE6 }, { "END" , 0xE7 }, { "PGDN", 0xE8 },
};

static const char alphanumeric[] = "ZXCVBNM,./\0\0ASDFGHJKL;'\0QWERTYUIOP[]1234567890-=";

static uint8_t x11_code_to_hc_keys[256];

static void input_map_calc(Display* dpy){
	XkbDescPtr xkb = XkbGetKeyboard(dpy, XkbAllComponentsMask, XkbUseCoreKbd);

	for(int i = 0; i < xkb->max_key_code; ++i){
		const char* name = xkb->names->keys[i].name;

		if(i > 0xff)
			break;

		// alphanumeric -> uppercase ascii
		if(name[0] == 'A' && (name[1] >= 'B' && name[1] <= 'E')){
			int x = atoi(name+2) - 1;
			if(x >= 0 && x <= 12){
				int y = name[1] - 'B';
				char c = alphanumeric[y * 12 + x];
				if(c){
					x11_code_to_hc_keys[i] = c;
					goto next;
				}
			}
		}

		// function keys -> 0xFX
		if(name[0] == 'F' && name[1] == 'K'){
			int x = atoi(name+2);
			if(x >= 1 && x <= 12){
				x11_code_to_hc_keys[i] = 0xF0 + x;
				goto next;
			}
		}

		// special keys
		for(int j = 0; j < countof(xkb_map_special); ++j){
			const struct xkb_km* p = xkb_map_special + j;

			if(memcmp(p->name, name, XkbKeyNameLength) == 0){
				x11_code_to_hc_keys[i] = p->hc_key;
				goto next;
			}
		}

		next:;
	}

	XkbFreeKeyboard(xkb, 0, True);
}

void input_xkb_init(Display* dpy){
	int maj = XkbMajorVersion, min = XkbMinorVersion;
	int op, ev, err;

	if(!XkbQueryExtension(dpy, &op, &ev, &err, &maj, &min)){
		fprintf(stderr, "FATAL: Failed to initialise XKB.\n");
		exit(1);
	}

	Bool dar_enabled = 0;
	XkbSetDetectableAutoRepeat(dpy, True, &dar_enabled);

	input_map_calc(dpy);
}

static void input_xkb_push(XKeyEvent* ev, _Bool pressed){
	if(hc_key_count >= 32)
		return;

	int keycode = ev->keycode;
	if(keycode < 0 || keycode > 0xff)
		return;

	uint8_t key = x11_code_to_hc_keys[keycode];
	if(key == 0)
		return;

	hc_keys[hc_key_count++] = (hc_key){
		.pressed = pressed,
		.key = key,
	};
}

void input_xkb_press(XKeyEvent* ev, _Bool check_esc){
	if(check_esc){
		KeySym sym = XLookupKeysym(ev, 0);
		if(sym == XK_Escape){
			exit(0);
		}
	}
	input_xkb_push(ev, 1);
}

void input_xkb_release(XKeyEvent* ev){
	input_xkb_push(ev, 0);
}
