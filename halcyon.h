#pragma once        /*****************/
#define WIN_W 427   /* H A L C Y O N */
#define WIN_H 240   /*****************/
#include <stdint.h>

typedef struct hc_v2  { int_fast16_t x, y; } hc_v2;
typedef struct hc_vx  { int_fast16_t x, y; float tx, ty; } hc_vx;
typedef struct hc_tex { int_fast16_t w, h; void* data; } hc_tex;

void hc_init    (const char* name);
void hc_point   (hc_v2 a,                   uint32_t rgb);
void hc_line    (hc_v2 a, hc_v2 b,          uint32_t rgb);
void hc_tri     (hc_v2 a, hc_v2 b, hc_v2 c, uint32_t fill, uint32_t border);
void hc_tri_tex (hc_vx verts[static 3], const hc_tex* tex);
int  hc_scanout (int n);
void hc_scroll  (hc_v2 p);
void hc_finish  (uint8_t flags); // TODO: hc_v2* [2] param for redraw rect

enum { HC_CLEAR=1, HC_NODRAW=2, HC_NOESC=4 };

extern struct  hc_s {unsigned pitch:15,rst:1,vol:4,duty:3,:1,pan:2;} hc_snd[4];
extern uint8_t hc_wave[/*16*/];
