#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct mus_pat {
	uint16_t  speed;
	uint16_t  loops;
	uint16_t* music;
} mus_pat;

#define M_INS(ins)         (uint16_t)((1u << 14u) | ((ins) & 0x3f))
#define M_PLY(note, ticks) (uint16_t)((2u << 14u) | (((note) & 0x3f) << 8u) | ((ticks) & 0xff))
#define M_END 0

typedef void (*mus_instr)(uint16_t note, uint16_t ticks);

typedef struct mus_track {
	mus_pat** patterns;
	size_t    count;
} mus_track;

typedef struct mus_song {
	mus_instr instruments[64];
	mus_track tracks[4];
} mus_song;

void mus_init  (mus_song* song);
void mus_play  (void);
void mus_yield (void);

#define sq0 (hc_snd + 0)
#define sq1 (hc_snd + 1)
#define wav (hc_snd + 2)
#define nse (hc_snd + 3)
