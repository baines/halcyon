#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/Xlibint.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wchar.h>
#include <math.h>
#include <errno.h>
#include <poll.h>
#include "halcyon.h"

struct pres_select_req {
	uint8_t  type, subtype;
	uint16_t length;
	uint32_t evid, window, mask;
};

struct pres_req {
	uint8_t  type, subtype;
	uint16_t length;
	uint32_t window, pixmap, crap[11];
	uint64_t divisor, remainder;
};

static int pres_op;
static int shm_ev_base;
static XShmSegmentInfo shm_info;
static XImage* img;
static Pixmap pix;
static Window win;
static XVisualInfo xvi;
static Display* dpy;
static void* imgmem;
static Atom close_atom;
static int fd_index_x11;

extern struct pollfd* g_pollfds;
extern size_t g_npollfds;

extern void input_xkb_init    (Display*);
extern void input_xkb_press   (XKeyEvent*, _Bool check_esc);
extern void input_xkb_release (XKeyEvent*);

void video_x11_init(const char* name, size_t w, size_t h){
	dpy = XOpenDisplay(NULL);

	assert(dpy);
	assert(XShmQueryExtension(dpy));
	assert(XMatchVisualInfo(dpy, DefaultScreen(dpy), 24, TrueColor, &xvi));

	img = XShmCreateImage(
		dpy,
		xvi.visual,
		xvi.depth,
		ZPixmap,
		NULL,
		&shm_info,
		w, h
	);

	shm_info.shmid = shmget(
		IPC_PRIVATE,
		img->bytes_per_line * img->height,
		IPC_CREAT | 0600
	);

	imgmem = shm_info.shmaddr = img->data = shmat(shm_info.shmid, 0, 0);
	XShmAttach(dpy, &shm_info);

	shmctl(shm_info.shmid, IPC_RMID, NULL);

	XSetWindowAttributes swa = {
		.background_pixel = BlackPixel(dpy, xvi.screen),
		.colormap = XCreateColormap(dpy, RootWindow(dpy, xvi.screen), xvi.visual, AllocNone),
		.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask,
	};

	win = XCreateWindow(
		dpy, RootWindow(dpy, xvi.screen),
		0, 0, w, h,
		0, xvi.depth, InputOutput, xvi.visual,
		CWBackPixel | CWEventMask, &swa
	);

	XStoreName(dpy, win, name ?: "H A L C Y O N");

	XMapRaised(dpy, win);

	XSizeHints* xsh = XAllocSizeHints();
	xsh->flags = PMinSize | PMaxSize;
	xsh->min_width  = xsh->max_width  = w;
	xsh->min_height = xsh->max_height = h;

	XSetWMNormalHints(dpy, win, xsh);
	XFree(xsh);

	close_atom = XInternAtom(dpy, "WM_DELETE_WINDOW", 0);
	XSetWMProtocols(dpy, win, &close_atom, 1);

	int ev, err;
	XQueryExtension(dpy, "Present", &pres_op, &ev, &err);
	pres_op = XInitExtension(dpy, "Present")->major_opcode;

	pix = XCreatePixmap(dpy, win, w, h, xvi.depth);

	shm_ev_base = XShmGetEventBase(dpy);

	fd_index_x11 = g_npollfds;
	g_pollfds = realloc(g_pollfds, sizeof(*g_pollfds) * ++g_npollfds);
	assert(g_pollfds);

	g_pollfds[fd_index_x11] = (struct pollfd){
		.fd = ConnectionNumber(dpy),
		.events = POLLIN,
	};

	input_xkb_init(dpy);
}

void hc_backend_scanline(unsigned scan_ctr, uint32_t* linebuf, int off){
	scan_ctr <<= 1;
	uint32_t (*wr)[img->width] = imgmem;

	for(size_t x = 0; x < img->width - off; x+=2){
		wr[scan_ctr][x+off] = wr[scan_ctr][x+off+1] = linebuf[x>>1];
	}

	memcpy(wr[scan_ctr+1], wr[scan_ctr], img->width*4);
}

void video_x11_frame(void){
	XShmPutImage(dpy, pix, DefaultGC(dpy, xvi.screen), img, 0, 0, 0, 0, img->width, img->height, 1);
	XFlush(dpy);
}

void video_x11_event(uint8_t flags){
	if(!(g_pollfds[fd_index_x11].revents & POLLIN))
		return;

	for(int n = XEventsQueued(dpy, QueuedAfterReading); n; --n){
		XEvent ev;
		XNextEvent(dpy, &ev);

		if(ev.type == ClientMessage && ev.xclient.data.l[0] == close_atom){
			exit(0);
		}

		else if(ev.type == KeyPress){
			input_xkb_press(&ev.xkey, !(flags & HC_NOESC));
		}

		else if(ev.type == KeyRelease){
			input_xkb_release(&ev.xkey);
		}

		// TODO: handle keyboard remap event

		else if(ev.type == shm_ev_base + ShmCompletion) {
			struct pres_req* req = _XGetRequest(dpy, pres_op, sizeof(*req));
			memset(req->crap, 0, sizeof(req->crap));
			req->subtype = 1;
			req->window = win;
			req->pixmap = pix;
			req->divisor = 1;
			req->remainder = 0;
			XFlush(dpy);

			if(flags & HC_CLEAR){
				memset(imgmem, 0, img->width * img->height * 4);
			}
		}
	}
}
