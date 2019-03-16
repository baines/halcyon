#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "halcyonix.h"
#include "hc_backend.h"

hc_key hc_keys[32];
int    hc_key_count;

extern void* hc_vram;

void hc_video_init  (void);
void hc_audio_init  (void);
void hc_audio_frame (uint8_t flags);
void hc_video_frame (uint8_t flags);

void hc_init(const char* name){
	puts("Created with HALCYON v" HC_VERSION);

	hc_video_init();
	hc_audio_init();

	hc_backend_init(name, WIN_W << 1, WIN_H << 1);
}

void hc_finish(uint8_t flags){
	hc_video_frame(flags);
	hc_audio_frame(flags);

	memset(hc_keys, 0, sizeof(hc_keys));
	hc_key_count = 0;

	hc_backend_frame(flags);

	if(flags & HC_CLEAR){
		asmemset(hc_vram, 0, WIN_W*WIN_H);
	}
}
