#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/timerfd.h>
#include "halcyon.h"
#include "../hc_backend.h"

void video_x11_init  (const char* name, size_t w, size_t h);
void video_x11_frame (void);
void video_x11_event (uint8_t flags);

void audio_alsa_init  (void);
void audio_alsa_frame (void);
void audio_alsa_event (uint8_t flags);

struct pollfd* g_pollfds;
size_t         g_npollfds;

static int fd_index_timer;
static int timer_started;

void hc_backend_init(const char* name, size_t w, size_t h){

	fd_index_timer = g_npollfds;
	g_pollfds = realloc(g_pollfds, sizeof(*g_pollfds) * ++g_npollfds);
	assert(g_pollfds);

	int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

	g_pollfds[fd_index_timer] = (struct pollfd){
		.fd = timer_fd,
		.events = POLLIN,
	};

	video_x11_init(name, w, h);
	audio_alsa_init();
}

void hc_backend_frame(uint8_t flags){

	if(!(flags & HC_NODRAW))
		video_x11_frame();

	if(!timer_started){
		timer_started = 1;

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
		timerfd_settime(g_pollfds[fd_index_timer].fd, 0, &spec, NULL);
	}

	audio_alsa_frame();

	for(;;){
		if(poll(g_pollfds, g_npollfds, -1) == -1)
			continue;

		if(g_pollfds[fd_index_timer].revents & POLLIN){
			uint64_t tmp;
			ssize_t ret;

			do {
				ret = read(g_pollfds[fd_index_timer].fd, &tmp, sizeof(tmp));
			} while(ret == -1 && errno != EAGAIN);

			break;
		}

		video_x11_event(flags);
		audio_alsa_event(flags);
	}
}
