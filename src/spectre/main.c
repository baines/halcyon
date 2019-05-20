#include <complex.h>
#include <tgmath.h>
#include <stdio.h>
#include <string.h>
#include <fftw3.h>
#include <alsa/asoundlib.h>
#include <wchar.h>

#define WIN_W 271
#define WIN_H 90

#include "hc0_linux.h"

#define N 800
#define ALSA_PERIOD N
#define BG 0xff111111

_Static_assert(N <= ALSA_PERIOD, "unhandled case");

static snd_pcm_t* alsarec;

static complex double W[N/2];
static double K[N];

void dft_init(void){
	for(int i = 0; i < N/2; ++i){
		W[i] = cexpf(-2.0f*M_PI*I*i/(double)N);
	}

	for(int i = 0; i < N; ++i){
		K[i] = 0.402
			 - 0.498 * cos((2*M_PI*i)/(double)(N-1))
			 + 0.098 * cos((4*M_PI*i)/(double)(N-1))
			 - 0.001 * cos((6*M_PI*i)/(double)(N-1))
			 ;
	}
}

static double window(int i, double x){
	return x * K[i];
}

void setup_alsa(const char* device){
	int err;

#define CHK(fn, ...) if((err = fn(__VA_ARGS__)) < 0){ fprintf(stderr, "%s: %d %s\n", #fn, err, snd_strerror(err)); exit(1); }

	CHK(snd_pcm_open, &alsarec, device, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);

	snd_pcm_hw_params_t*           hw;
	CHK(snd_pcm_hw_params_malloc, &hw);

	CHK(snd_pcm_hw_params_any             , alsarec, hw);
	CHK(snd_pcm_hw_params_set_access      , alsarec, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
	CHK(snd_pcm_hw_params_set_format      , alsarec, hw, SND_PCM_FORMAT_FLOAT);
	CHK(snd_pcm_hw_params_set_rate        , alsarec, hw, 48000, 0);
	CHK(snd_pcm_hw_params_set_channels    , alsarec, hw, 2);
	CHK(snd_pcm_hw_params_set_buffer_size , alsarec, hw, 1000);
	CHK(snd_pcm_hw_params                 , alsarec, hw);

#undef CHK

	snd_pcm_hw_params_free(hw);
	snd_pcm_start(alsarec);
}

int main(int argc, char** argv){
	hc_init("SPECTRE");
	dft_init();

	complex double* cfreq = fftw_alloc_complex(N/2+1);
	double* samples = fftw_alloc_real(N);

	fftw_plan plan = fftw_plan_dft_r2c_1d(N, samples, cfreq, FFTW_ESTIMATE);

	float sbuf1[ALSA_PERIOD*2];
	float sbuf2[ALSA_PERIOD*2];

	double prevabs[N/2];

	setup_alsa(argc > 1 ? argv[1] : "default");

	for(;;){
		wmemset(hc_vram, BG, WIN_W*WIN_H);
		memset(sbuf1, 0, sizeof(sbuf1));

		for(;;){
			int n = snd_pcm_readi(alsarec, sbuf2, ALSA_PERIOD);

			if(n == -EAGAIN)
				break;

			if(n < 0)
				continue;

			int amt = n * 2;
			int cnt = sizeof(sbuf1) - amt * 4;

			memmove(sbuf1, sbuf1 + amt, cnt);
			memcpy (sbuf1 + (cnt / 4), sbuf2, amt * 4);
		}

		for(int i = 0; i < ALSA_PERIOD; ++i){
			samples[i] = (sbuf1[2*i+0] + sbuf1[2*i+1]) / 2.0f;
		}

		for(int i = 0; i < N; ++i){
			samples[i] = window(i, samples[i]);
		}

		fftw_execute(plan);

		int index = 0;
		float inc = 0.0;
		float rem = 0.0;

		// enter magic numbers \o/

		for(int i = 0; ; ++i){
			double count = pow(1.1, i);
			inc += count;
			double sample = rem;

			while(inc >= 1.0){
				sample += cabsf(cfreq[index++]);
				inc -= 1.0;

				if(index >= N/2-1)
					goto done;
			}

			double left = cabsf(cfreq[index++]);
			sample += left * inc;
			rem = left * (1.0 - inc);
			inc -= 1.0;

			sample /= count;

			double a = pow(sample, 0.2+(index*2.0/(double)N)) * 32.0f;
			double b = a * 0.4 + prevabs[i] * 0.6;
			int h = (int)b;

			for(int j = 0; j < 7; ++j){
				hc_line((hc_v2){ 8*i+j, WIN_H }, (hc_v2){ 8*i+j, WIN_H - h }, 0xffcd8523);
			}

			prevabs[i] = b;
		}
done:

		for(int i = 0; i < 240; i += 2){
			hc_line((hc_v2){ 0, i }, (hc_v2){ WIN_W, i }, BG);
		}

		hc_finish(HC_CLEAR);
	}
}
