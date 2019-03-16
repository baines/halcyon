#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "halcyonix.h"
#include "hc_backend.h"

void* hc_vram;

typedef hc_v2 v2;
typedef hc_vx vx;
typedef float v2f __attribute__((vector_size(8)));

static int scan_ctr;
static v2  scroll;

// UTILS

static void plot(int x, int y, uint32_t c){
	uint32_t (*p)[WIN_W] = hc_vram;
	if(x >= 0 && y >= 0 && y < WIN_H && x < WIN_W){
		p[y][x] = c;
	}
}

static inline void swapv2(v2* restrict a, v2* restrict b){
	v2 tmp = *a; *a = *b; *b = tmp;
}

static inline void swapvx(vx* restrict a, vx* restrict b){
	vx tmp = *a; *a = *b; *b = tmp;
}

static inline void swapvx2(v2* restrict a, v2* restrict b){
	swapvx((vx*)a, (vx*)b);
}

// POINT

void hc_point(v2 xy, uint32_t rgb){
	plot(xy.x, xy.y, rgb | 0xff000000);
}

// LINE

typedef struct ln_state {
	uint_fast16_t ax2, ay2;
	int_fast16_t  err, inc;
} ln_state;

typedef void (*plot_fn)(int, int, uint32_t);
typedef void (*swap_fn)(v2* restrict a, v2* restrict b);
typedef void (*line_fn)(v2* restrict v[static restrict 4], intptr_t fill);

__attribute__((always_inline, flatten))
static inline void line_setup(v2* restrict a, v2* restrict b, ln_state* restrict s, swap_fn swap){
	int_fast16_t  dx = b->x - a->x, dy = b->y - a->y;
	uint_fast16_t ax = __builtin_abs(dx) << 1, ay = __builtin_abs(dy) << 1;

	*s = (ln_state){
		.ax2 = ax, .ay2 = ay,
		.inc = (((dx >= 0) ^ (dy < 0)) << 1) - 1,
	};

	if(dy < 0){
		swap(a, b);
	}
}

__attribute__((always_inline, flatten))
static inline bool line_step_x(v2* restrict a, v2* restrict b, ln_state* restrict s, plot_fn plot, uint32_t color){
	while(a->x != b->x){
		plot(a->x, a->y, color);
		a->x += s->inc;

		if(s->err > 0){
			++a->y;
			s->err -= s->ax2;
			s->err += s->ay2;
			return true;
		}

		s->err += s->ay2;
	}
	return false;
}

__attribute__((always_inline, flatten))
static inline bool line_step_y(v2* restrict a, v2* restrict b, ln_state* restrict s, plot_fn plot, uint32_t color){
	if(a->y != b->y){
		plot(a->x, a->y, color);
		++a->y;

		if(s->err > 0){
			a->x += s->inc;
			s->err -= s->ay2;
		}

		s->err += s->ax2;
		return true;
	}
	return false;
}

__attribute__((flatten))
void hc_line(v2 a, v2 b, uint32_t rgb){
	ln_state s;
	line_setup(&a, &b, &s, &swapv2);

	if(s.ax2 > s.ay2){
		s.err = s.ay2 - (s.ax2 >> 1);
		while(line_step_x(&a, &b, &s, &plot, rgb));
	} else {
		s.err = s.ax2 - (s.ay2 >> 1);
		while(line_step_y(&a, &b, &s, &plot, rgb));
	}
}

// TRIANGLE

__attribute__((always_inline, flatten))
static inline void half_tri(v2* restrict p[static restrict 4], intptr_t fill, uint32_t border, plot_fn plot, line_fn line, swap_fn swap){
	ln_state s0, s1;
	line_setup(p[0], p[1], &s0, swap);
	line_setup(p[3], p[2], &s1, swap);

	if(s0.ax2 > s0.ay2){
		s0.err = s0.ay2 - (s0.ax2 >> 1);

		if(s1.ax2 > s1.ay2){
			s1.err = s1.ay2 - (s1.ax2 >> 1);

			for(;;){
				if(!line_step_x(p[0], p[1], &s0, plot, border) || !line_step_x(p[3], p[2], &s1, plot, border))
					return;
				line(p, fill);
			}
		} else {
			s1.err = s1.ax2 - (s1.ay2 >> 1);

			for(;;){
				if(!line_step_x(p[0], p[1], &s0, plot, border) || !line_step_y(p[3], p[2], &s1, plot, border))
					return;
				line(p, fill);
			}
		}
	} else {
		s0.err = s0.ax2 - (s0.ay2 >> 1);

		if(s1.ax2 > s1.ay2){
			s1.err = s1.ay2 - (s1.ax2 >> 1);

			for(;;){
				if(!line_step_y(p[0], p[1], &s0, plot, border) || !line_step_x(p[3], p[2], &s1, plot, border))
					return;
				line(p, fill);
			}
		} else {
			s1.err = s1.ax2 - (s1.ay2 >> 1);

			for(;;){
				if(!line_step_y(p[0], p[1], &s0, plot, border) || !line_step_y(p[3], p[2], &s1, plot, border))
					return;
				line(p, fill);
			}
		}
	}
}

__attribute__((always_inline))
static inline void fastline(v2* restrict v[static restrict 4], intptr_t fill){
	int_fast16_t x0 = v[0]->x, x1 = v[3]->x, y = v[0]->y;

	if(x0 == x1){
		plot(x0, y, fill);
		return;
	}

	if(x0 > x1){
		int tmp = x0;
		x0 = x1;
		x1 = tmp;
	}

	if(y > 0 && y < WIN_H && x0 > 0 && x1 > 0 && x0 < WIN_W && x1 < WIN_W){
		uint32_t (*p)[WIN_W] = hc_vram;
		int max = WIN_W - x0;
		int diff = x1 - x0;
		if(diff > max)
			diff = max;

		asmemset(&p[y][x0], fill, diff);
	}
}

static void fill_half_tri(v2 p0, v2 p1, v2 p2, uint32_t fill, uint32_t border){
	v2 p3 = p0;
	v2* restrict arr[] = { &p0, &p1, &p2, &p3 };
	half_tri(arr, fill, border, &plot, &fastline, &swapv2);
}

void hc_tri(v2 a, v2 b, v2 c, uint32_t fill, uint32_t border){
	v2 p[3] = { a, b, c };

	if(p[0].y > p[1].y) swapv2(p+0, p+1);
	if(p[1].y > p[2].y) swapv2(p+1, p+2);
	if(p[0].y > p[1].y) swapv2(p+0, p+1);

	int nx = p[0].x + (p[1].y - p[0].y) / (float)(p[2].y - p[0].y) * (p[2].x - p[0].x);
	v2 p4 = { nx, p[1].y };

	fill_half_tri(p[0], p[1], p4, fill, border);
	fill_half_tri(p[2], p[1], p4, fill, border);
}

// TEXTURED TRIANGLE

typedef struct tex_info {
	const hc_tex* tex;
	float height;
} tex_info;

__attribute__((always_inline))
static inline void texline(v2* restrict _v[static restrict 4], intptr_t arg){
	const vx** v = (const vx**)_v;
	const tex_info* info = (tex_info*)arg;
	const hc_tex* tex = info->tex;
	const uint32_t (*txp)[tex->w] = tex->data;

	const float t = 1.0f - ((v[1]->y - v[0]->y) / info->height);

	v2f tex0 = (v2f){ v[0]->tx, v[0]->ty } + t * ((v2f){ v[1]->tx, v[1]->ty } - (v2f){ v[0]->tx, v[0]->ty });
	v2f tex1 = (v2f){ v[3]->tx, v[3]->ty } + t * ((v2f){ v[2]->tx, v[2]->ty } - (v2f){ v[3]->tx, v[3]->ty });

	int_fast16_t x0 = v[0]->x, x1 = v[3]->x;
	if(x0 > x1){
		int_fast16_t tmp = x0;
		x0 = x1;
		x1 = tmp;

		v2f vtmp;
		memcpy(&vtmp, &tex0, sizeof(tex0));
		memcpy(&tex0, &tex1, sizeof(tex0));
		memcpy(&tex1, &vtmp, sizeof(tex0));
	}

	int w = x1 - x0;
	float w1 = 1.0f / (float)w;
	const v2f sz = { tex->w, tex->h };

	for(int i = 0; i < w; ++i){
		v2f p = (tex0 + (i * w1) * (tex1 - tex0)) * sz;
		uint32_t c = txp[(int)p[1] % tex->h][(int)p[0] % tex->w];

		if(!(c >> 24))
			continue;

		uint8_t r = (c >> 0 ) & 0xff;
		uint8_t g = (c >> 8 ) & 0xff;
		uint8_t b = (c >> 16) & 0xff;
		c = 0xff000000 | (r << 16) | (g << 8) | b;

		plot(x0 + i, v[0]->y, c);
	}
}

static void plot_nop(int x, int y, uint32_t c){}
static void fill_half_tri_tex(vx v[static 3], const hc_tex* restrict tex){
	vx p3 = v[0];
	v2* arr[] = { (v2*)(v+0), (v2*)(v+1), (v2*)(v+2), (v2*)&p3 };

	tex_info info = {
		.tex = tex,
		.height = fabsf((float)v[1].y - (float)v[0].y),
	};

	half_tri(arr, (intptr_t)&info, 0, &plot_nop, &texline, &swapvx2);
}

void hc_tri_tex(vx verts[static 3], const hc_tex* tex){
	if(verts[0].y > verts[1].y) swapvx(verts+0, verts+1);
	if(verts[1].y > verts[2].y) swapvx(verts+1, verts+2);
	if(verts[0].y > verts[1].y) swapvx(verts+0, verts+1);

	float ty = (verts[1].y - verts[0].y) / (float)(verts[2].y - verts[0].y);
	int   nx = verts[0].x + ty * (verts[2].x - verts[0].x);

	float d = (float)(verts[2].x - verts[0].x);
	float tx = fabsf(d) < FLT_EPSILON ? 0.0f : (nx - verts[0].x) / d;

	vx v4 = {
		.x = nx,
		.y = verts[1].y,
		.tx = verts[0].tx + tx * (verts[2].tx - verts[0].tx),
		.ty = verts[0].ty + ty * (verts[2].ty - verts[0].ty),
	};

	fill_half_tri_tex((vx[]){ verts[0], verts[1], v4 }, tex);
	fill_half_tri_tex((vx[]){ verts[2], verts[1], v4 }, tex);
}

// OTHER EXTERN FUNCS

void hc_video_init(void){
	hc_vram = calloc(WIN_W*WIN_H, sizeof(uint32_t));
}

int hc_scanout(int n){
	uint32_t (*p)[WIN_W] = hc_vram;

	for(int i = 0; i < n; ++i){
		if(scan_ctr+i >= WIN_H)
			break;

		hc_backend_scanline(scan_ctr+i, p[scan_ctr+i], scroll.x);
	}

	scan_ctr += n;

	return WIN_H - scan_ctr;
}

void hc_scroll(v2 p){
	scroll = p;
}

void hc_video_frame(uint8_t flags){
	if(!(flags & HC_NODRAW)){
		hc_scanout(WIN_H - scan_ctr);
	}
	scan_ctr = 0;
}
