#include <math.h>
#include <poll.h>
#include <alsa/asoundlib.h>
#include "../hc_backend.h"
#include "halcyonix.h"

#define HC_PERIOD_SIZE (HC_BUFFER_SIZE / HC_CHANNELS)

extern struct pollfd* g_pollfds;
extern int            g_npollfds;

static snd_pcm_t*     pcm;
static snd_rawmidi_t* midi;

static int midi_fd_index, midi_fd_count;
static uint8_t midi_prev_cmd;

void audio_alsa_init(void){
	snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);

	snd_pcm_hw_params_t*                hw;
	snd_pcm_hw_params_malloc          (&hw);
	snd_pcm_hw_params_any             (pcm, hw);
	snd_pcm_hw_params_set_access      (pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format      (pcm, hw, SND_PCM_FORMAT_FLOAT);
	snd_pcm_hw_params_set_channels    (pcm, hw, HC_CHANNELS);
	snd_pcm_hw_params_set_rate        (pcm, hw, HC_SAMPLE_RATE, 0);
	snd_pcm_hw_params_set_buffer_size (pcm, hw, HC_PERIOD_SIZE * 3);
	snd_pcm_hw_params_set_period_size (pcm, hw, HC_PERIOD_SIZE, 1);
	snd_pcm_hw_params                 (pcm, hw);
	snd_pcm_hw_params_free            (hw);

	snd_pcm_sw_params_t*                    sw;
	snd_pcm_sw_params_malloc              (&sw);
	snd_pcm_sw_params_current             (pcm, sw);
	snd_pcm_sw_params_set_start_threshold (pcm, sw, HC_PERIOD_SIZE * 2.25);
	snd_pcm_sw_params                     (pcm, sw);
	snd_pcm_sw_params_free                (sw);

	snd_rawmidi_open (&midi, NULL, "virtual", 0);

	midi_fd_count = snd_rawmidi_poll_descriptors_count(midi);
	midi_fd_index = g_npollfds;

	g_pollfds = realloc(g_pollfds, (g_npollfds + midi_fd_count) * sizeof(struct pollfd));
	snd_rawmidi_poll_descriptors(midi, g_pollfds + midi_fd_index, midi_fd_count);
	g_npollfds += midi_fd_count;
}

void audio_alsa_event(uint8_t flags){
	uint16_t revents;
	snd_rawmidi_poll_descriptors_revents(midi, g_pollfds + midi_fd_index, midi_fd_count, &revents);

	if(!(revents & POLLIN))
		return;

	uint8_t data[3];
	snd_rawmidi_read(midi, data, 3);

	uint8_t cmd = midi_prev_cmd;
	uint8_t* p = data;

	if(*p >= 0x80){
		cmd = *p++;
	}

	hc_midi[hc_midi_count++] = (hc_midi_ev){
		.cmd = cmd,
		.data = { p[0], p[1] }
	};

	midi_prev_cmd = cmd;
}

void audio_alsa_frame(void){
	int err = snd_pcm_writei(pcm, hc_samples_mixed, HC_PERIOD_SIZE);
	if(err < 0){
		snd_pcm_recover(pcm, err, 1);
	}
}
