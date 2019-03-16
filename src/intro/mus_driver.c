#include "mus_driver.h"
#include <ucontext.h>
#include <setjmp.h>

struct track {
	mus_track* mus;
	ucontext_t ctx;
	jmp_buf    tick_jmp;
	uint16_t   tick_limit;
	uint8_t    stack[8192];
	uint16_t   pat_idx;
	uint16_t   pat_loop_ctr;
	uint16_t   code_idx;
	uint8_t    instrument;
};

static ucontext_t main_ctx;
static struct track tracks[4];
static struct track* t;
static mus_song* song;

enum {
	CMD_SET_INSTRUMENT = 1,
	CMD_PLAY_NOTE = 2,
};

static void mus_track_fn(void){
	for(;;){
		mus_pat* p = t->mus->patterns[t->pat_idx];
		uint16_t op = p->music[t->code_idx++];

		if(op == M_END){
			t->code_idx = 0;
			if(++t->pat_loop_ctr > p->loops){
				t->pat_loop_ctr = 0;
				t->pat_idx = (t->pat_idx + 1) % t->mus->count;
			}
			continue;
		}

		int cmd_id = (op >> 14);
		if(cmd_id == CMD_SET_INSTRUMENT){
			t->instrument = (op & 0x3f);
		}

		else if(cmd_id == CMD_PLAY_NOTE){
			uint16_t note = (op >> 8) & 0x3f;
			t->tick_limit = (op & 0xff) * p->speed;

			if(setjmp(t->tick_jmp) == 0){
				song->instruments[t->instrument](note, t->tick_limit);
			}
		}
	}
}

static void mus_track_init(int i){
	struct track* c = tracks + i;

	getcontext(&c->ctx);
	c->ctx.uc_stack.ss_sp = c->stack;
	c->ctx.uc_stack.ss_size = sizeof(c->stack);
	makecontext(&c->ctx, &mus_track_fn, 0);
}

static void mus_play_track(int i){
	t = tracks + i;
	swapcontext(&main_ctx, &t->ctx);
}

void mus_yield(){
	swapcontext(&t->ctx, &main_ctx);
	if(t->tick_limit && --t->tick_limit == 0){
		longjmp(t->tick_jmp, 1);
	}
}

void mus_init(mus_song* s){
	song = s;

	for(int i = 0; i < 4; ++i){
		if(song->tracks[i].count){
			mus_track_init(i);
			tracks[i].mus = song->tracks + i;
		}
	}
}

void mus_play(void){
	if(!song)
		return;

	for(int i = 0; i < 4; ++i){
		if(song->tracks[i].count){
			mus_play_track(i);
		}
	}
}
