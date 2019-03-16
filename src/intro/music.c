#include "halcyon.h"
#include "mus_driver.h"

#define MAX(a,b) ((a) >= (b) ? (a) : (b))
#define MIN(a,b) ((a) <  (b) ? (a) : (b))

static void ins_bd(uint16_t note, uint16_t ticks){
	sq0->duty = 0;
	sq0->pitch = 0x1100;
	sq0->vol = 10;

	nse->pitch = 0xA00;
	nse->duty = 0;
	nse->vol = 9;
	nse->rst = 1;

	for(;;){
		for(int i = 0; i < 4; ++i){
			sq0->pitch = MAX(0, (int)sq0->pitch - 0x100);
			nse->pitch = MAX(0, (int)nse->pitch - 0x40);
			mus_yield();
		}
		nse->vol = MAX(0, (int)nse->vol - 5);
		sq0->vol = MAX(0, (int)sq0->vol - 3);
	}
}

static void ins_sq(uint16_t note, uint16_t ticks){
	sq1->vol = 4;
	sq1->pitch = ((note) << 8);
	sq1->duty = 4;

	for(;;){
		mus_yield();
		mus_yield();
		sq1->vol = MAX(4, sq1->vol - 2);
		sq1->duty = 1;
	}
}

static void ins_hat(uint16_t note, uint16_t ticks){
	nse->vol = 6;
	nse->pitch = 0x4500;
	nse->duty = 0;

	for(;;){
		mus_yield();
		nse->duty += 2;
		nse->pitch -= 0x700;
		nse->vol = MAX(0, nse->vol-1);
	}
}

static void ins_hh2(uint16_t note, uint16_t ticks){
	nse->vol = 3;
	nse->pitch = 0x5300;
	nse->duty = 5;
	nse->rst = 1;

	for(;;){
		mus_yield();
		nse->duty = 3;
		nse->rst = 1;
		mus_yield();
		nse->vol = MAX(0, nse->vol - 1);
	}
}

static mus_pat drum_pat = {
	.speed = 12,
	.music = (uint16_t[]){
		M_INS(0), M_PLY(0, 2), M_INS(2), M_PLY(0, 3),
		M_INS(0), M_PLY(0, 1), M_INS(2), M_PLY(0, 2),

		M_END,
	}
};

static mus_pat melody_pat = {
	.speed = 6,
	.music = (uint16_t[]){
		M_INS(1), M_PLY(26, 1), M_PLY(31, 1), M_PLY(34, 1), M_PLY(38, 1),
		M_INS(1), M_PLY(26, 1), M_PLY(31, 1), M_PLY(34, 1), M_PLY(38, 1),
		M_INS(1), M_PLY(26, 1), M_PLY(31, 1), M_PLY(34, 1), M_PLY(38, 1),
		M_INS(1), M_PLY(26, 1), M_PLY(31, 1), M_PLY(34, 1), M_PLY(38, 1),

		M_INS(1), M_PLY(23, 1), M_PLY(35, 1), M_PLY(39, 1), M_PLY(42, 1),
		M_INS(1), M_PLY(23, 1), M_PLY(35, 1), M_PLY(39, 1), M_PLY(42, 1),
		M_INS(1), M_PLY(23, 1), M_PLY(35, 1), M_PLY(39, 1), M_PLY(42, 1),
		M_INS(1), M_PLY(23, 1), M_PLY(35, 1), M_PLY(39, 1), M_PLY(42, 1),

		M_INS(1), M_PLY(20, 1), M_PLY(31, 1), M_PLY(36, 1), M_PLY(39, 1),
		M_INS(1), M_PLY(20, 1), M_PLY(31, 1), M_PLY(36, 1), M_PLY(39, 1),
		M_INS(1), M_PLY(20, 1), M_PLY(31, 1), M_PLY(36, 1), M_PLY(39, 1),
		M_INS(1), M_PLY(20, 1), M_PLY(31, 1), M_PLY(36, 1), M_PLY(39, 1),

		M_INS(1), M_PLY(22, 1), M_PLY(31, 1), M_PLY(34, 1), M_PLY(41, 1),
		M_INS(1), M_PLY(22, 1), M_PLY(31, 1), M_PLY(34, 1), M_PLY(41, 1),
		M_INS(1), M_PLY(22, 1), M_PLY(31, 1), M_PLY(34, 1), M_PLY(41, 1),
		M_INS(1), M_PLY(22, 1), M_PLY(31, 1), M_PLY(34, 1), M_PLY(41, 1),

		M_END
	}
};

static mus_pat melody2_pat = {
	.speed = 6,
	.music = (uint16_t[]){

		M_INS(1), M_PLY(26, 1), M_PLY(31, 1), M_PLY(34, 1), M_PLY(38, 1),
		M_INS(1), M_PLY(26, 1), M_PLY(31, 1), M_PLY(34, 1), M_PLY(38, 1),
		M_INS(1), M_PLY(26, 1), M_PLY(31, 1), M_PLY(34, 1), M_PLY(38, 1),
		M_INS(1), M_PLY(26, 1), M_PLY(31, 1), M_PLY(34, 1), M_PLY(38, 1),

		M_INS(1), M_PLY(26, 1), M_PLY(31, 1), M_PLY(34, 1), M_PLY(38, 1),
		M_INS(1), M_PLY(26, 1), M_PLY(31, 1), M_PLY(34, 1), M_PLY(38, 1),
		M_INS(1), M_PLY(26, 1), M_PLY(31, 1), M_PLY(34, 1), M_PLY(38, 1),
		M_INS(1), M_PLY(22, 1), M_PLY(26, 1), M_PLY(30, 1), M_PLY(34, 1),

		M_END
	}
};

static mus_pat hh2_pat = {
	.speed = 6,
	.music = (uint16_t[]){
		M_INS(3), M_PLY(0, 1), M_END,
	},
};

static mus_song song = {
	.instruments = {
		[0] = &ins_bd,
		[1] = &ins_sq,
		[2] = &ins_hat,
		[3] = &ins_hh2,
	},
	.tracks = {
		[2] = {
			.patterns = (mus_pat*[]){ &drum_pat },
			.count = 1,
		},
		[1] = {
			.patterns = (mus_pat*[]){ &melody_pat, &melody_pat, &melody2_pat, &melody2_pat },
			.count = 4,
		},
		[0] = {
			.patterns = (mus_pat*[]){ &hh2_pat },
			.count = 1,
		}
	}
};

void music_init(void){ mus_init(&song); }
void music_play(void){ mus_play(); }
