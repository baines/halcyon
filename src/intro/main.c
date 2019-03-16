#include <math.h>
#include <stdbool.h>
#include "linalg.h"
#include "halcyon.h"
#include "sb.h"
#include "ico.h"

extern bool tga_load(const void* data, size_t len, hc_tex* tex);
extern void music_init(void);
extern void music_play(void);

struct tri { v4 a, b, c; };
static sb(struct tri) tris;
static int frame;

int zsort(struct tri* a, struct tri* b){
	float z0 = (a->a[2] + a->b[2] + a->c[2]) / 3.0f;
	float z1 = (b->a[2] + b->b[2] + b->c[2]) / 3.0f;
	return (z1*10000) - (z0 * 10000);
}

extern char _binary_font_tga_start[];
extern char _binary_font_tga_end[];

extern char _binary_text_start[];
extern char _binary_text_end[];

static hc_tex font_tex;
static void draw_char(uint8_t c, int x, int y, int w, int h){

	float tx0 = (1.0f/(float)(font_tex.w/font_tex.h)) * (c - '0'),
	      tx1 = (1.0f/(float)(font_tex.w/font_tex.h)) * (c - '0' + 1);

	hc_vx quad[] = {
		{ x+w*0, y+0, tx0, 0 },
		{ x+w*1, y+0, tx1, 0 },
		{ x+w*0, y+h, tx0, 1 },
		{ x+w*1, y+0, tx1, 0 },
		{ x+w*1, y+h, tx1, 1 },
		{ x+w*0, y+h, tx0, 1 },
	};

	hc_tri_tex(quad+0, &font_tex);
	hc_tri_tex(quad+3, &font_tex);
}

int main(void){

	hc_init("H A L C Y O N");
	music_init();

	const size_t sz = _binary_font_tga_end - _binary_font_tga_start;
	tga_load(_binary_font_tga_start, sz, &font_tex);

	mat4 proj;
	frustum(proj, -1, 1, -0.625, 0.625, 1, 100);

	float rot = 0.0f;

	for(;;){
		music_play();

		// BACKGROUND

		for(int i = 0; i < WIN_H; ++i){
			int diff = WIN_H - i;
			if(diff & 1){
				uint32_t c = 0xff002222 | ((uint8_t)~diff << 16);
				hc_line((hc_v2){ 0, i }, (hc_v2){ WIN_W-16, i }, c);
			}
		}

		// ICOSAHEDRON
		{
			float s, c;
			sincosf(rot, &s, &c);

			mat4 rotm = {
				{ c   , 0 , s  , 0 },
				{ -s*s, c , s*c, 0 },
				{ -s*c, -s, c*c, 0 },
				{ 0   , 0 , 0  , 1 },
			};

			mat4 tran = {
				{ 1, 0, 0, 0   },
				{ 0, 1, 0, 0   },
				{ 0, 0, 1, s-3 },
				{ 0, 0, 0, 1   },
			};

			mat4 mv;
			mat4_mul(mv, tran, rotm);

			mat4 mvp;
			mat4_mul(mvp, proj, mv);

			if(tris){
				__sbh(tris)->used = 0;
			}

			for(int i = 0; i < 20; ++i){
				v4 a = mat4_dp4(mvp, vdata[tindices[i][0]]);
				v4 b = mat4_dp4(mvp, vdata[tindices[i][1]]);
				v4 c = mat4_dp4(mvp, vdata[tindices[i][2]]);

				struct tri t = {
					.a = a / a[3],
					.b = b / b[3],
					.c = c / c[3],
				};
				sb_push(tris, t);
			}

			qsort(tris, sb_count(tris), sizeof(struct tri), (int(*)())&zsort);

			sb_each(t, tris){
				uint32_t fill   = 0xffff4444;
				uint32_t border = 0xffffffff;

				float w = WIN_W/2.0f;
				float h = WIN_H/2.0f;

				// backface cull
				v4 dir = cross(t->b - t->a, t->b - t->c);
				if(dir[2] < 0)
					continue;

				hc_tri(
					(hc_v2){ w+w*t->a[0], h+h*t->a[1] },
					(hc_v2){ w+w*t->b[0], h+h*t->b[1] },
					(hc_v2){ w+w*t->c[0], h+h*t->c[1] },
					fill, border
				);
			}
		}

		// SCROLLING TEXT
		{
			static const char* msgp = _binary_text_start;
			static int msgx0 = WIN_W;
			static int msgy = 30;

			int x = msgx0;
			int i = 0;

			do {
				const char* p = msgp + i;
				if(p >= _binary_text_end){
					p = _binary_text_start + (p - _binary_text_end);
				}

				uint8_t c = *p;
				if(c > ' '){
					int j = p - _binary_text_start;
					float dt = ((20*j/2 + 4*frame) % WIN_H) / (float)WIN_H * M_PI * 2.0;

					int xd = 4.0 * cosf(dt);
					int yd = 12 + 6.0 * sinf(dt);

					draw_char(c, x-xd, msgy-yd, 20+2*xd, 20+2*yd);
				}

				x += 20;
				++i;
			} while(x < WIN_W);

			msgx0 -= 4;
			if(msgx0 < -20){
				msgx0 += 20;
				if(++msgp == _binary_text_end){
					msgp = _binary_text_start;
				}
			}
		}

		// DRAW WITH SINEWAVE EFFECT

		for(int i = 0; i < WIN_H; ++i){
			int dt  = (i + 4*frame) % WIN_H;
			int off = 16 + 16.0*sinf(dt/(float)WIN_H*M_PI*2.0);

			hc_scroll((hc_v2){ off, 0 });
			hc_scanout(1);
		}

		// END OF FRAME

		hc_finish(HC_CLEAR);

		rot += 0.01;
		if(rot > 2*M_PI)
			rot -= 2*M_PI;

		frame++;
	}
}
