#include <math.h>
#include <stdbool.h>
#include <assert.h>
#include "linalg.h"
#include "halcyonix.h"
#include "sb.h"

#define LOG2(X) ((unsigned)(32 - __builtin_clz((X)) - 1))

union tri {
    struct {
        v4 a, b, c;
    };
    v4 v[3];
};

int r_dir, x_dir, y_dir, z_dir;
float r, x, y, z = 1;

void dir_key(hc_key k, uint8_t k1, uint8_t k2, int* dir) {
    if(k.key == k1){
        *dir = k.pressed ? -1 : 0;
    } else if(k.key == k2){
        *dir = k.pressed ? +1 : 0;
    }
}

union tri pdiv(union tri in) {
    return (union tri){
        .a = in.a / in.a[3],
        .b = in.b / in.b[3],
        .c = in.c / in.c[3],
    };
}

v4 wcross(v4 a, v4 b){
    return (v4){
        a[1] * b[3] - a[3] * b[1],
        a[3] * b[0] - a[0] * b[3],
        a[0] * b[1] - a[1] * b[0],
    };
}

v4 wcrossx(v4 a, v4 b){
    return (v4){
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        1.0f,
        a[0] * b[1] - a[1] * b[0],
    };
}

int main(void){

    hc_init("clipping");

    mat4 proj;
    frustum(proj, -1, 1, -0.625, 0.625, 1, 100);

    for(;;){

        // keyboard controls:
        //   up/down    arrow keys = increase/decrease size (i.e. camera z position)
        //   left/right arrow keys = rotate
        //   w a s d               = move triangle around

        for(int i = 0; i < hc_key_count; ++i) {
            dir_key(hc_keys[i], 0xA3, 0xA1, &r_dir);
            dir_key(hc_keys[i], 0xA0, 0xA2, &z_dir);
            dir_key(hc_keys[i], 'W', 'S', &y_dir);
            dir_key(hc_keys[i], 'A', 'D', &x_dir);
        }

        x += x_dir * 0.01;
        y += y_dir * 0.01;
        z += z_dir * 0.01;
        r += r_dir * 0.01;

        float s, c;
        sincosf(r, &s, &c);

        mat4 mv = {
            { c, -s, 0, x },
            { s,  c, 0, y },
            { 0,  0, 1, -z },
            { 0,  0, 0, 1 },
        };

        mat4 mvp;
        mat4_mul(mvp, proj, mv);

        {
            // set up a roughly equilateral input triangle
            union tri orig = {
                mat4_dp4(mvp, (v4){ +0.0, -0.2309, +0.0, +1.0 }),
                mat4_dp4(mvp, (v4){ +0.2, +0.1155, +0.0, +1.0 }),
                mat4_dp4(mvp, (v4){ -0.2, +0.1155, +0.0, +1.0 }),
            };

            union tri tris[20];
            union tri* p = tris;

            uint32_t fill   = 0xff008080;
            uint32_t border = 0xffffffff;

            // triangle clipping algorithm
            //
            // not sure if it has a standard known name.
            // it uses masks and the following properties of 2d clipspace (homogeneous) coords:
            //
            // point x point = line vector between them
            // vec   x vec   = intersection point of 2 vecs
            //
            // where "x" is some kind of cross-product, see wcross and wcrossx funcs
            //
            // it clips each of the 3 lines of the triangle, always going clockwise around the 4 screen edges
            // each triangle edge can add up to 2 points to the output.
            //
            // the output points are ordered to be drawn as a triangle fan, starting from point[0]
            //
            // the literal corner cases are handled using a separate mask with a different set of bits,
            // which tracks intersections with the edges outside the screen (i.e. 2 per corner)
            //

            uint8_t vmask[3] = {};

            // each area of the screen is given a mask:
            //      2
            //   +-----+
            //   |     |
            // 1 |  0  | 4
            //   |     |
            //   +-----+
            //      8
            //
            // corner positions become 3, 6, 12, 9 - i.e. ORing together the two sides (all multiples of 3 interestingly)

            for(int vx = 0; vx < 3; ++vx) {
                if(orig.v[vx][0] < -orig.v[vx][3])
                    vmask[vx] |= 1;
                else if(orig.v[vx][0] > orig.v[vx][3])
                    vmask[vx] |= 4;

                if(orig.v[vx][1] < -orig.v[vx][3])
                    vmask[vx] |= 2;
                else if(orig.v[vx][1] > orig.v[vx][3])
                    vmask[vx] |= 8;
            }

            // homogeneous coords (vectors) of the 4 screen axes: left, top, right, bottom
            static const v4 axes[] = {
                { +1, 0, 1 },
                { 0, +1, 1 },
                { -1, 0, 1 },
                { 0, -1, 1 },
            };

            // starting offsets for the clockwise scren axis check for each mask 0-12
            static const int starts[] = {
                0, 0, 1, 0, 2, 0, 1, 0, 3, 3, 1, 1, 2
            };

            // corner_test_masks: masks for each corner. this is using a different bitset than for axes above (8bit instead of 4)
            // so that each corner has 2 bits as follows:
            //
            //    2     32
            //    |     |
            // 1--+-----+-- 4
            //    |     |
            //    |     |
            //    |     |
            // 16-+-----+-- 64
            //    |     |
            //    8    128
            //
            // the order may seem bizzare. it was based on going clockwise left, top, right, bottom from 1-128 starting with "negative" sides based on x/y coords
            // where y is negative towards the top of the screen, x is negative towards the left.
            //
            // i.e. 1 is negative y on the left
            //      2 is negative x on the top
            //      4 is negative y on the right
            //      8 is negative x on the bottom
            //
            //      then it goes around the clock again for positive instead of negative
            //
            // there is probably a better order that would simplify some of the shifting etc, but this is the one I came up with.
            // and now that the program is working (I think...) I don't feel like figuring out another one (yet?..)
            //
            static const uint8_t corner_test_masks[] = {
                3, 3, 36, 24, 24, 36, 192, 192
            };

            // stores the bits for the 8 bit mask above, used to test if a corner should be added.
            uint8_t exterior_edge_mask = 0;

            // homogeneous coords (points) of the 4 screen corners corresponding to the above masks
            static const v4 corners[] = {
                { -1, -1, 0, 1 },
                { -1, -1, 0, 1 },
                { +1, -1, 0, 1 },
                { -1, +1, 0, 1 },
                { -1, +1, 0, 1 },
                { +1, -1, 0, 1 },
                { +1, +1, 0, 1 },
                { +1, +1, 0, 1 },
            };

            v4 points[20];
            v4* q = points;

            printf("--------------------\n");

            // for each of the 3 triangle edges...
            for(int i = 0; i < 3; ++i) {
                int j = (1 << i) & 3; // i.e. (i+1)%3

                printf("--\n");

                printf("masks %d/%d:\ni: %#x\nj: %#x\n", i, j, vmask[i], vmask[j]);

                // both points of the edge are inside -> add current point and move onto next edge
                uint8_t mask = vmask[i] | vmask[j];
                if(mask == 0) {
                    *q++ = orig.v[i];
                    printf("[%d] both inside\n", i);
                    continue;
                }

                uint8_t xmask = vmask[i] ^ vmask[j];

                // if xor mask is zero, i.e. vmask[i] == vmask[j], then points are in the same "outside 8th" of the screen
                // and can't add any visible points, continue
                if(xmask == 0) {
                    continue;
                }

                uint8_t amask = vmask[i] & vmask[j];

                // if the AND mask is not zero, then both points are on the same "outside 4th" side of the screen.
                // it can't add any visible points EXCEPT corners. we check the masks to find the corner cases,
                // then continue without doing any of the cross product stuff.

                if(amask) {
                    const uint8_t side = LOG2(amask);
                    const bool swap = vmask[i] > vmask[j];

                    // we need to check positive before negative if the "from" mask is bigger than "to"
                    // otherwise negative first. this is to keep the output points in correct fan order.

                    // TODO: use a function or something, this is pretty silly.
                    if(swap)
                        goto pos_check;

neg_check:
                    if(xmask & 0x3) {
                        const uint8_t id = side;

                        exterior_edge_mask |= (1 << id);
                        printf("update ext mask - v1: %d, %d (%#x)\n", id, exterior_edge_mask, exterior_edge_mask);

                        uint8_t corner_test_mask = corner_test_masks[id];
                        if((exterior_edge_mask & corner_test_mask) == corner_test_mask) {
                            printf("add corner - v1 %d\n", id);
                            *q++ = corners[id];
                        }
                    }

                    if(swap)
                        continue;

pos_check:
                    if(xmask & 0xc) {
                        const uint8_t id = side + 4;

                        exterior_edge_mask |= (1 << id);
                        printf("update ext mask + v1: %d, %d (%#x)\n", id, exterior_edge_mask, exterior_edge_mask);

                        uint8_t corner_test_mask = corner_test_masks[id];
                        if((exterior_edge_mask & corner_test_mask) == corner_test_mask) {
                            printf("add corner + v1 %d\n", id);
                            *q++ = corners[id];
                        }
                    }

                    if(swap)
                        goto neg_check;

                    continue;
                }

                // get the vector between the two points
                v4 line = wcross(orig.v[i], orig.v[j]);

                // which screen edge to check first, then continuing clockwise
                int start = starts[vmask[i]];

                // mask 0 means it's a point inside the screen
                if(vmask[i] == 0) {
                    printf("[%d] vmi 0\n", i);
                    *q++ = orig.v[i];
                    start = starts[vmask[j]];
                }

                // check for intersections against the 4 edges
                for(int z = 0; z < 4; ++z) {
                    int e = (z + start) & 3;

                    // intersection not possible according to mask, next edge
                    if(!(mask & (1 << e)))
                        continue;

                    // get the intersection point with the axis and the triangle edge
                    v4 p = wcrossx(axes[e], line);
                    printf("[%d] %d p x/y/w: %.2f/%.2f/%.2f\n", i, e, p[0], p[1], p[3]);

                    const float w = __builtin_fabs(p[3]);
                    const int xyi = ((e^1)&1);
                    const float xy = p[xyi];
                    const int samesign = signbit(p[3]) == signbit(p[xyi]);

                    // if the intersection point is visible, addd it
                    // otherwise test if we need to add a corner

                    if(xy <= w && xy >= -w) {
                        printf("add v %x\n", e);
                        *q++ = p;
                    } else {
                        const uint8_t id = (e^1)+(e&2)+samesign*2; // I just brute forced the truth table for this one...
                        exterior_edge_mask |= (1 << id);
                        printf("update ext mask v2: %d, %d (%#x)\n", id, exterior_edge_mask, exterior_edge_mask);

                        uint8_t corner_test_mask = corner_test_masks[id];
                        if((exterior_edge_mask & corner_test_mask) == corner_test_mask) {
                            printf("add corner v2 %d\n", id);
                            *q++ = corners[id];
                        }
                    }
                }
            }

            printf("%ld\n", q - points);

            // create triangle fan and do the perspective divide
            for(v4* v = points; v + 3 <= q; ++v) {
                union tri t = { .v = { points[0], v[1], v[2] } };
                *p++ = pdiv(t);
            }

            // draw the fan on the screen

            const float w = WIN_W/2.0f;
            const float h = WIN_H/2.0f;

            const float w2 = (WIN_W-122)/2.0f;
            const float h2 = (WIN_H-80)/2.0f;

            for(union tri* t = tris; t < p; ++t) {
                hc_tri(
                    (hc_v2){ w+w2*t->a[0], h+h2*t->a[1] },
                    (hc_v2){ w+w2*t->b[0], h+h2*t->b[1] },
                    (hc_v2){ w+w2*t->c[0], h+h2*t->c[1] },
                    fill, border
                );
            }
        }

        // inner screen border
        hc_line((hc_v2){ 61 , 40  }, (hc_v2){ 366, 40  }, 0xffffffff);
        hc_line((hc_v2){ 61 , 200 }, (hc_v2){ 366, 200 }, 0xffffffff);
        hc_line((hc_v2){ 61 , 40  }, (hc_v2){ 61 , 200 }, 0xffffffff);
        hc_line((hc_v2){ 366, 40  }, (hc_v2){ 366, 200 }, 0xffffffff);

        hc_finish(HC_CLEAR);
    }
}
