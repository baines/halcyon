#pragma once
/* Halcyon ZERO (single-header video-only) -- Linux */
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <assert.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/Xlibint.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/timerfd.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>

/* user configuation */
#ifndef WIN_W
	#define WIN_W 427
#endif

#ifndef WIN_H
	#define WIN_H 240
#endif

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

void* hc_vram;

enum { HC_CLEAR=1, HC_NODRAW=2, HC_NOESC=4 };

// begin private-ish stuff

#define HC_VERSION      "0.1 ZERO"
#define HC_FPS          60UL

__attribute__((always_inline))
static inline void asmemset(register void* _dest, uint32_t _num, size_t _count){
	register void* dest asm("rdi") = _dest;
	register uint32_t num asm("eax") = _num;
	register size_t count asm("rcx") = _count;
	asm("rep stosl" :: "r" (num), "r" (count), "r" (dest));
}

static struct {
	int pres_op;
	int shm_ev_base;
	XShmSegmentInfo shm_info;
	XImage* img;
	Pixmap pix;
	Window win;
	XVisualInfo xvi;
	Display* dpy;
	void* imgmem;
	Atom close_atom;
	int scan_ctr;
	hc_v2 scroll;
	struct pollfd pollfds[2];
	size_t npollfds;
	int timer_started;
} _hc_priv;

void hc_init(const char* name){
	hc_vram = calloc(WIN_W*WIN_H, sizeof(uint32_t));

	int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

	_hc_priv.pollfds[0] = (struct pollfd){
		.fd = timer_fd,
		.events = POLLIN,
	};

	int w = WIN_W << 1, h = WIN_H << 1;

	_hc_priv.dpy = XOpenDisplay(NULL);

	assert(_hc_priv.dpy);
	assert(XShmQueryExtension(_hc_priv.dpy));
	assert(XMatchVisualInfo(_hc_priv.dpy, DefaultScreen(_hc_priv.dpy), 24, TrueColor, &_hc_priv.xvi));

	_hc_priv.img = XShmCreateImage(
		_hc_priv.dpy,
		_hc_priv.xvi.visual,
		_hc_priv.xvi.depth,
		ZPixmap,
		NULL,
		&_hc_priv.shm_info,
		w, h
	);

	_hc_priv.shm_info.shmid = shmget(
		IPC_PRIVATE,
		_hc_priv.img->bytes_per_line * _hc_priv.img->height,
		IPC_CREAT | 0600
	);

	_hc_priv.imgmem = _hc_priv.shm_info.shmaddr = _hc_priv.img->data = shmat(_hc_priv.shm_info.shmid, 0, 0);
	XShmAttach(_hc_priv.dpy, &_hc_priv.shm_info);

	shmctl(_hc_priv.shm_info.shmid, IPC_RMID, NULL);

	XSetWindowAttributes swa = {
		.background_pixel = BlackPixel(_hc_priv.dpy, _hc_priv.xvi.screen),
		.colormap = XCreateColormap(_hc_priv.dpy, RootWindow(_hc_priv.dpy, _hc_priv.xvi.screen), _hc_priv.xvi.visual, AllocNone),
		.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask,
	};

	_hc_priv.win = XCreateWindow(
		_hc_priv.dpy, RootWindow(_hc_priv.dpy, _hc_priv.xvi.screen),
		0, 0, w, h,
		0, _hc_priv.xvi.depth, InputOutput, _hc_priv.xvi.visual,
		CWBackPixel | CWEventMask, &swa
	);

	XStoreName(_hc_priv.dpy, _hc_priv.win, name ?: "HALCYON ZERO");

	XMapRaised(_hc_priv.dpy, _hc_priv.win);

	XSizeHints* xsh = XAllocSizeHints();
	xsh->flags = PMinSize | PMaxSize;
	xsh->min_width  = xsh->max_width  = w;
	xsh->min_height = xsh->max_height = h;

	XSetWMNormalHints(_hc_priv.dpy, _hc_priv.win, xsh);
	XFree(xsh);

    _hc_priv.close_atom = XInternAtom(_hc_priv.dpy, "WM_DELETE_WINDOW", 0);
    XSetWMProtocols(_hc_priv.dpy, _hc_priv.win, &_hc_priv.close_atom, 1);

	int ev, err;
	XQueryExtension(_hc_priv.dpy, "Present", &_hc_priv.pres_op, &ev, &err);
	_hc_priv.pres_op = XInitExtension(_hc_priv.dpy, "Present")->major_opcode;

	_hc_priv.pix = XCreatePixmap(_hc_priv.dpy, _hc_priv.win, w, h, _hc_priv.xvi.depth);
	_hc_priv.shm_ev_base = XShmGetEventBase(_hc_priv.dpy);

	_hc_priv.pollfds[1] = (struct pollfd){
		.fd = ConnectionNumber(_hc_priv.dpy),
		.events = POLLIN,
	};
}

static void _hc_video_x11_frame(void){
	XShmPutImage(
		_hc_priv.dpy,
		_hc_priv.pix,
		DefaultGC(_hc_priv.dpy, _hc_priv.xvi.screen),
		_hc_priv.img,
		0, 0, 0, 0,
		_hc_priv.img->width,
		_hc_priv.img->height,
		1
	);
	XFlush(_hc_priv.dpy);
}

static void _hc_video_x11_event(uint8_t flags){
	if(!(_hc_priv.pollfds[1].revents & POLLIN))
		return;

	struct pres_req {
		uint8_t  type, subtype;
		uint16_t length;
		uint32_t window, pixmap, crap[11];
		uint64_t divisor, remainder;
	};

	for(int n = XEventsQueued(_hc_priv.dpy, QueuedAfterReading); n; --n){
		XEvent ev;
		XNextEvent(_hc_priv.dpy, &ev);

		if(ev.type == ClientMessage && ev.xclient.data.l[0] == _hc_priv.close_atom){
			exit(0);
		}

		else if(ev.type == KeyPress && !(flags & HC_NOESC)){
			KeySym sym = XLookupKeysym(&ev.xkey, 0);
			if(sym == XK_Escape){
				exit(0);
			}
		}

		else if(ev.type == _hc_priv.shm_ev_base + ShmCompletion) {
			struct pres_req* req = _XGetRequest(_hc_priv.dpy, _hc_priv.pres_op, sizeof(*req));
			memset(req->crap, 0, sizeof(req->crap));
			req->subtype = 1;
			req->window = _hc_priv.win;
			req->pixmap = _hc_priv.pix;
			req->divisor = 1;
			req->remainder = 0;
			XFlush(_hc_priv.dpy);

			if(flags & HC_CLEAR){
				memset(_hc_priv.imgmem, 0, _hc_priv.img->width * _hc_priv.img->height * 4);
			}
		}
	}
}


void hc_finish(uint8_t flags){
	if(!(flags & HC_NODRAW)){
		hc_scanout(WIN_H - _hc_priv.scan_ctr);
	}
	_hc_priv.scan_ctr = 0;

	if(!(flags & HC_NODRAW))
		_hc_video_x11_frame();

	if(!_hc_priv.timer_started){
		_hc_priv.timer_started = 1;

		static const struct itimerspec spec = {
			.it_value = {
				.tv_sec = 0,
				.tv_nsec = 1UL * 1000UL * 1000UL,
			},
			.it_interval = {
				.tv_sec = 0,
				.tv_nsec = (1000000000UL / HC_FPS),
			}
		};
		timerfd_settime(_hc_priv.pollfds[0].fd, 0, &spec, NULL);
	}

	for(;;){
		if(poll(_hc_priv.pollfds, 2, -1) == -1)
			continue;

		if(_hc_priv.pollfds[0].revents & POLLIN){
			uint64_t tmp;
			ssize_t ret;

			do {
				ret = read(_hc_priv.pollfds[0].fd, &tmp, sizeof(tmp));
			} while(ret == -1 && errno != EAGAIN);

			break;
		}

		_hc_video_x11_event(flags);
	}
}

static void _hc_backend_scanline(unsigned scan_ctr, uint32_t* linebuf, int off){
	scan_ctr <<= 1;
	uint32_t (*wr)[_hc_priv.img->width] = _hc_priv.imgmem;

	for(size_t x = 0; x < _hc_priv.img->width - off; x+=2){
		wr[scan_ctr][x+off] = wr[scan_ctr][x+off+1] = linebuf[x>>1];
	}

	memcpy(wr[scan_ctr+1], wr[scan_ctr], _hc_priv.img->width*4);
}

int hc_scanout(int n){
	uint32_t (*p)[WIN_W] = hc_vram;

	for(int i = 0; i < n; ++i){
		if(_hc_priv.scan_ctr+i >= WIN_H)
			break;

		_hc_backend_scanline(_hc_priv.scan_ctr+i, p[_hc_priv.scan_ctr+i], _hc_priv.scroll.x);
	}

	_hc_priv.scan_ctr += n;

	return WIN_H - _hc_priv.scan_ctr;
}

void hc_scroll(hc_v2 p){
	_hc_priv.scroll = p;
}

static void _hc_plot(int x, int y, uint32_t c){
	uint32_t (*p)[WIN_W] = hc_vram;
	if(x >= 0 && y >= 0 && y < WIN_H && x < WIN_W){
		p[y][x] = c;
	}
}

static inline void _hc_swaphc_v2(hc_v2* restrict a, hc_v2* restrict b){
	hc_v2 tmp = *a; *a = *b; *b = tmp;
}

static inline void _hc_swaphc_vx(hc_vx* restrict a, hc_vx* restrict b){
	hc_vx tmp = *a; *a = *b; *b = tmp;
}

static inline void _hc_swaphc_vx2(hc_v2* restrict a, hc_v2* restrict b){
	_hc_swaphc_vx((hc_vx*)a, (hc_vx*)b);
}

// POINT

void hc_point(hc_v2 xy, uint32_t rgb){
	_hc_plot(xy.x, xy.y, rgb | 0xff000000);
}

// LINE

typedef struct {
	uint_fast16_t ax2, ay2;
	int_fast16_t  err, inc;
} _hc_ln_state;

typedef void (*_hc_plot_fn)(int, int, uint32_t);
typedef void (*_hc_swap_fn)(hc_v2* restrict a, hc_v2* restrict b);
typedef void (*_hc_line_fn)(hc_v2* restrict v[static restrict 4], intptr_t fill);

__attribute__((always_inline, flatten))
static inline void _hc_line_setup(hc_v2* restrict a, hc_v2* restrict b, _hc_ln_state* restrict s, _hc_swap_fn swap){
	int_fast16_t  dx = b->x - a->x, dy = b->y - a->y;
	uint_fast16_t ax = __builtin_abs(dx) << 1, ay = __builtin_abs(dy) << 1;

	*s = (_hc_ln_state){
		.ax2 = ax, .ay2 = ay,
		.inc = (((dx >= 0) ^ (dy < 0)) << 1) - 1,
	};

	if(dy < 0){
		swap(a, b);
	}
}

__attribute__((always_inline, flatten))
static inline _Bool _hc_line_step_x(hc_v2* restrict a, hc_v2* restrict b, _hc_ln_state* restrict s, _hc_plot_fn plot, uint32_t color){
	while(a->x != b->x){
		plot(a->x, a->y, color);
		a->x += s->inc;

		if(s->err > 0){
			++a->y;
			s->err -= s->ax2;
			s->err += s->ay2;
			return 1;
		}

		s->err += s->ay2;
	}
	plot(a->x, a->y, color);
	return 0;
}

__attribute__((always_inline, flatten))
static inline _Bool _hc_line_step_y(hc_v2* restrict a, hc_v2* restrict b, _hc_ln_state* restrict s, _hc_plot_fn plot, uint32_t color){
	if(a->y != b->y){
		plot(a->x, a->y, color);
		++a->y;

		if(s->err > 0){
			a->x += s->inc;
			s->err -= s->ay2;
		}

		s->err += s->ax2;
		return 1;
	}
	plot(a->x, a->y, color);
	return 0;
}

__attribute__((flatten))
void hc_line(hc_v2 a, hc_v2 b, uint32_t rgb){
	_hc_ln_state s;
	_hc_line_setup(&a, &b, &s, &_hc_swaphc_v2);

	if(s.ax2 > s.ay2){
		s.err = s.ay2 - (s.ax2 >> 1);
		while(_hc_line_step_x(&a, &b, &s, &_hc_plot, rgb));
	} else {
		s.err = s.ax2 - (s.ay2 >> 1);
		while(_hc_line_step_y(&a, &b, &s, &_hc_plot, rgb));
	}
}

// TRIANGLE

__attribute__((always_inline, flatten))
static inline void _hc_half_tri(hc_v2* restrict p[static restrict 4], intptr_t fill, uint32_t border, _hc_plot_fn plot, _hc_line_fn line, _hc_swap_fn swap){
	_hc_ln_state s0, s1;
	_hc_line_setup(p[0], p[1], &s0, swap);
	_hc_line_setup(p[3], p[2], &s1, swap);

	if(s0.ax2 > s0.ay2){
		s0.err = s0.ay2 - (s0.ax2 >> 1);

		if(s1.ax2 > s1.ay2){
			s1.err = s1.ay2 - (s1.ax2 >> 1);

			for(;;){
				if(!_hc_line_step_x(p[0], p[1], &s0, plot, border) || !_hc_line_step_x(p[3], p[2], &s1, plot, border))
					return;
				line(p, fill);
			}
		} else {
			s1.err = s1.ax2 - (s1.ay2 >> 1);

			for(;;){
				if(!_hc_line_step_x(p[0], p[1], &s0, plot, border) || !_hc_line_step_y(p[3], p[2], &s1, plot, border))
					return;
				line(p, fill);
			}
		}
	} else {
		s0.err = s0.ax2 - (s0.ay2 >> 1);

		if(s1.ax2 > s1.ay2){
			s1.err = s1.ay2 - (s1.ax2 >> 1);

			for(;;){
				if(!_hc_line_step_y(p[0], p[1], &s0, plot, border) || !_hc_line_step_x(p[3], p[2], &s1, plot, border))
					return;
				line(p, fill);
			}
		} else {
			s1.err = s1.ax2 - (s1.ay2 >> 1);

			for(;;){
				if(!_hc_line_step_y(p[0], p[1], &s0, plot, border) || !_hc_line_step_y(p[3], p[2], &s1, plot, border))
					return;
				line(p, fill);
			}
		}
	}
}

__attribute__((always_inline))
static inline void _hc_fastline(hc_v2* restrict v[static restrict 4], intptr_t fill){
	int_fast16_t x0 = v[0]->x, x1 = v[3]->x, y = v[0]->y;

	if(x0 == x1){
		_hc_plot(x0, y, fill);
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

static void _hc_fill_half_tri(hc_v2 p0, hc_v2 p1, hc_v2 p2, uint32_t fill, uint32_t border){
	hc_v2 p3 = p0;
	hc_v2* restrict arr[] = { &p0, &p1, &p2, &p3 };
	_hc_half_tri(arr, fill, border, &_hc_plot, &_hc_fastline, &_hc_swaphc_v2);
}

void hc_tri(hc_v2 a, hc_v2 b, hc_v2 c, uint32_t fill, uint32_t border){
	hc_v2 p[3] = { a, b, c };

	if(p[0].y > p[1].y) _hc_swaphc_v2(p+0, p+1);
	if(p[1].y > p[2].y) _hc_swaphc_v2(p+1, p+2);
	if(p[0].y > p[1].y) _hc_swaphc_v2(p+0, p+1);

	int nx = p[0].x + (p[1].y - p[0].y) / (float)(p[2].y - p[0].y) * (p[2].x - p[0].x);
	hc_v2 p4 = { nx, p[1].y };

	_hc_fill_half_tri(p[0], p[1], p4, fill, border);
	_hc_fill_half_tri(p[2], p[1], p4, fill, border);
}

// TEXTURED TRIANGLE

typedef struct {
	const hc_tex* tex;
	float height;
} _hc_tex_info;

__attribute__((always_inline))
static inline void _hc_texline(hc_v2* restrict _v[static restrict 4], intptr_t arg){
	typedef float v2f __attribute__((vector_size(8)));

	const hc_vx** v = (const hc_vx**)_v;
	const _hc_tex_info* info = (_hc_tex_info*)arg;
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

		_hc_plot(x0 + i, v[0]->y, c);
	}
}

static void _hc_plot_nop(int x, int y, uint32_t c){}
static void _hc_fill_half_tri_tex(hc_vx v[static 3], const hc_tex* restrict tex){
	hc_vx p3 = v[0];
	hc_v2* arr[] = { (hc_v2*)(v+0), (hc_v2*)(v+1), (hc_v2*)(v+2), (hc_v2*)&p3 };

	_hc_tex_info info = {
		.tex = tex,
		.height = fabsf((float)v[1].y - (float)v[0].y),
	};

	_hc_half_tri(arr, (intptr_t)&info, 0, &_hc_plot_nop, &_hc_texline, &_hc_swaphc_vx2);
}

void hc_tri_tex(hc_vx verts[static 3], const hc_tex* tex){
	if(verts[0].y > verts[1].y) _hc_swaphc_vx(verts+0, verts+1);
	if(verts[1].y > verts[2].y) _hc_swaphc_vx(verts+1, verts+2);
	if(verts[0].y > verts[1].y) _hc_swaphc_vx(verts+0, verts+1);

	float ty = (verts[1].y - verts[0].y) / (float)(verts[2].y - verts[0].y);
	int   nx = verts[0].x + ty * (verts[2].x - verts[0].x);

	float d = (float)(verts[2].x - verts[0].x);
	float tx = fabsf(d) < FLT_EPSILON ? 0.0f : (nx - verts[0].x) / d;

	hc_vx v4 = {
		.x = nx,
		.y = verts[1].y,
		.tx = verts[0].tx + tx * (verts[2].tx - verts[0].tx),
		.ty = verts[0].ty + ty * (verts[2].ty - verts[0].ty),
	};

	_hc_fill_half_tri_tex((hc_vx[]){ verts[0], verts[1], v4 }, tex);
	_hc_fill_half_tri_tex((hc_vx[]){ verts[2], verts[1], v4 }, tex);
}
