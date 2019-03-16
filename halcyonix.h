#pragma once /* HALCYON INTERACTIVE EXTENSIONS */
#include "halcyon.h"

typedef struct hc_key { uint8_t pressed:1, _:7, key; } hc_key;
extern hc_key  hc_keys[32];
extern int     hc_key_count;

typedef struct    hc_midi_ev { uint8_t cmd, data[2]; } hc_midi_ev;
extern hc_midi_ev hc_midi[32];
extern int        hc_midi_count;

extern char*  hc_text;
