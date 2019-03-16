#pragma once

#include <stdint.h>
#include <stddef.h>

#define HC_VERSION      "0.1"

#define HC_FPS          60
#define HC_SAMPLE_RATE  48000
#define HC_CHANNELS     2
#define HC_BUFFER_SIZE  (HC_SAMPLE_RATE / HC_FPS * HC_CHANNELS)

void hc_backend_init     (const char* name, size_t w, size_t h);
void hc_backend_scanline (unsigned ctr, uint32_t* linebuf, int off);
void hc_backend_frame    (uint8_t flags);

extern float hc_samples_mixed [HC_BUFFER_SIZE];

__attribute__((always_inline))
static inline void asmemset(register void* _dest, uint32_t _num, size_t _count){
	register void* dest asm("rdi") = _dest;
	register uint32_t num asm("eax") = _num;
	register size_t count asm("rcx") = _count;
	asm("rep stosl" :: "r" (num), "r" (count), "r" (dest));
}
