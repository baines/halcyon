#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <limits.h>
#include "halcyonix.h"
#include "hc_backend.h"

#define NSAMPLES HC_BUFFER_SIZE
#define HIPASS_FACTOR 0.996336633f
#define XIIRT2 1.059463094f

#define CHAN_IDX_SQUARE0 0
#define CHAN_IDX_SQUARE1 1
#define CHAN_IDX_WAVE    2
#define CHAN_IDX_NOISE   3

#define PITCH_MIN 0x0080 // C-1 minus half semitone
#define PITCH_MAX 0x5480 // C-8 minus half semitone
#define SEMITONES ((PITCH_MAX >> 8) - (PITCH_MIN >> 8))

#define countof(x) (sizeof(x)/sizeof(*x))

float hc_samples_mixed[NSAMPLES];
float hc_samples[4][NSAMPLES];

// default sawtooth
uint8_t hc_wave[HC_SAMPLE_RATE/120] = {
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
	0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
};

hc_midi_ev hc_midi[32];
int        hc_midi_count;

struct hc_s hc_snd[4] = {
	[0 ... 3] = { .pan = 0b11 },
};

static struct chan_state {
	float cycles_per_sample;
	float samples_per_cycle;

	float cycle_counter;
	uint8_t duty_counter;

	float capacitor;
	int value;
} state[4];

static uint16_t lfsr = 0xffff;

static const uint16_t duty_masks[] = {
	0xFF00, 0xFF80, 0xFFC0, 0xFFE0, 0xFFF0, 0xFFF8, 0xFFFC, 0xFFFE
};

static float freq_table[SEMITONES + 2];

static float hipass(struct chan_state* c, float sample){
#if 0
	float out = sample - c->capacitor;
	c->capacitor = sample - out * HIPASS_FACTOR;
	return out;
#else
	return sample;
#endif
}

static bool chan_freq_set(struct chan_state* c, uint16_t pitch, float multiplier){
	if(pitch < PITCH_MIN || pitch >= PITCH_MAX)
		return false;

	int ex = pitch >> 8;
	float f0 = freq_table[ex], f1 = freq_table[ex+1];
	float freq = (f0 + (f1 - f0) * (float)((pitch & 0xFF) / 255.0f)) * 440.0f;

	//printf("playing freq %.2f\n", freq);

	c->cycles_per_sample = (freq / HC_SAMPLE_RATE) * multiplier;
	c->samples_per_cycle = 1.0f / c->cycles_per_sample;

	return true;
}

static bool chan_freq_advance(struct chan_state* c, float* cursor){
	float inc = c->cycles_per_sample - *cursor;
	c->cycle_counter += inc;

	if(c->cycle_counter > 1.0f){
		*cursor = c->cycles_per_sample - (c->cycle_counter - 1.0f);
		c->cycle_counter = 0.0f;
		return true;
	} else {
		*cursor = c->cycles_per_sample;
		return false;
	}
}

static uint8_t wave_sample(int idx){
	uint8_t val = hc_wave[idx/2];
	if(idx & 1){
		val &= 0xf;
	} else {
		val >>= 4;
	}
	return val;
}

static void chan_process_square(int index){
	struct chan_state* c = state + index;
	struct hc_s* s = hc_snd + index;

	if(s->rst){
		c->duty_counter = s->rst = 0;
	}

	if(!chan_freq_set(c, s->pitch, 16.0f))
		return;

	for(int i = 0; i < NSAMPLES; i += 2){
		float cursor = 0.0f, prev = 0.0f, sample = 0.0f;

		while(chan_freq_advance(c, &cursor)){
			c->duty_counter = (c->duty_counter + 1) & 15;
			sample += ((cursor - prev) * c->samples_per_cycle) * (float)c->value;
			c->value = (((duty_masks[s->duty] >> c->duty_counter) & 1) << 1) - 1;
			prev = cursor;
		}
		sample += ((cursor - prev) * c->samples_per_cycle) * (float)c->value;
		sample = hipass(c, sample * (s->vol / 15.0f));

		hc_samples[index][i+0] = sample * 0.25f * (float)!!(s->pan & 0b10);
		hc_samples[index][i+1] = sample * 0.25f * (float)!!(s->pan & 0b01);
	}
}

static void chan_process_wave(void){
	struct chan_state* c = state + CHAN_IDX_WAVE;
	struct hc_s* s = hc_snd + CHAN_IDX_WAVE;

	// 4-bit pcm playback easter egg
	const int limit = (s->duty & 0b100) ? (HC_SAMPLE_RATE/60) : 32;

	if(!chan_freq_set(c, s->pitch, (float)limit))
		return;

	if(s->rst){
		c->value = s->rst = 0;
	}

	for(int i = 0; i < NSAMPLES; i += 2){
		float cursor = 0.0f, prev = 0.0f, sample = 0.0f;
		uint8_t nibble = wave_sample(c->value);

		while(chan_freq_advance(c, &cursor)){
			c->value = (c->value + 1) % limit;
			sample += ((cursor - prev) * c->samples_per_cycle) * (float)nibble;
			nibble = wave_sample(c->value);
			prev = cursor;
		}
		sample += ((cursor - prev) * c->samples_per_cycle) * (float)nibble;
		sample = hipass(c, (sample / 15.0f) * (s->vol / 15.0f));

		hc_samples[CHAN_IDX_WAVE][i+0] = sample * 0.25f * (float)!!(s->pan & 0b10);
		hc_samples[CHAN_IDX_WAVE][i+1] = sample * 0.25f * (float)!!(s->pan & 0b01);
	}
}

static void chan_process_noise(void){
	struct chan_state* c = state + CHAN_IDX_NOISE;
	struct hc_s* s = hc_snd + CHAN_IDX_NOISE;

	if(!chan_freq_set(c, s->pitch, 8.0f))
		return;

	if(s->rst){
		lfsr = 0xffff;
		c->value = -1;
		s->rst = 0;
	}

	for(int i = 0; i < NSAMPLES; i += 2){
		float cursor = 0.0f, prev = 0.0f, sample = 0.0f;

		while(chan_freq_advance(c, &cursor)){
			lfsr = (lfsr << 1) | (c->value == 1);

			uint8_t tap0 = (lfsr >> (14 - s->duty)) & 1;
			uint8_t tap1 = (lfsr >> (13 - s->duty)) & 1;

			c->value = ((tap0 ^ tap1) << 1) - 1;

			sample += ((cursor - prev) * c->samples_per_cycle) * (float)c->value;
			prev = cursor;
		}
		sample += ((cursor - prev) * c->samples_per_cycle) * (float)c->value;
		sample = hipass(c, sample * (s->vol / 15.0f));

		hc_samples[CHAN_IDX_NOISE][i+0] = sample * 0.25f * (float)!!(s->pan & 0b10);
		hc_samples[CHAN_IDX_NOISE][i+1] = sample * 0.25f * (float)!!(s->pan & 0b01);
	}
}

void hc_audio_init(void){
	for(int i = 0; i < countof(freq_table); ++i){
		int ex = i - 46;
		freq_table[i] = powf(XIIRT2, ex);
	}
}

void hc_audio_frame(void){

	memset(hc_midi, 0, sizeof(hc_midi));
	hc_midi_count = 0;

	memset(hc_samples, 0, sizeof(hc_samples));

	chan_process_square(CHAN_IDX_SQUARE0);
	chan_process_square(CHAN_IDX_SQUARE1);
	chan_process_wave();
	chan_process_noise();

	memset(hc_samples_mixed, 0, sizeof(hc_samples_mixed));

	for(int i = 0; i < NSAMPLES; ++i){
		for(int c = 0; c < 4; ++c){
			hc_samples_mixed[i] += hc_samples[c][i];
		}
	}
}
