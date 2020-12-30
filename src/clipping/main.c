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
            // the literal corner cases are handled at the end - using the edge_mask, exit_index, inside vars
            // to track their state and insert extra vertices at the corners where needed for the output points
            // to represent the clipped filled polygon properly.
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

            // homogeneous coords (points) for each of the 4 corners: topleft, topright, bottomleft, bottomright (3, 6, 9, 12 masks)
            static const v4 corners[4] = {
                { -1, -1, 0, 1 },
                { +1, -1, 0, 1 },
                { -1, +1, 0, 1 },
                { +1, +1, 0, 1 },
            };

            // starting offsets for the clockwise scren axis check for each mask 0-12
            // -1 entries should not be possible to access (mask 5, 7, 10, 11)
            static const int starts[] = {
                0, 0, 1, 0, 2, -1, 1, -1, 3, 3, -1, -1, 2
            };

            v4 points[20];
            v4* q = points;

            // edge_mask tracks the state of intersections.
            // if it is not zero at the end of the axis checks, then we need to add some extra points at the corners.
            uint8_t edge_mask = 0;

            // exit_index holds the index into the output point array where the corresponding triangle edge left the screen.
            // it is used to know where to insert extra corner points at the end (since the output points are ordered).
            int exit_index[4] = {};

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

                // both outside -> skip this edge entirely, it can't add any visible points
                if(vmask[i] & vmask[j]) {
                    continue;
                }

                // get the vector between the two points
                v4 line = wcross(orig.v[i], orig.v[j]);

                bool inside = false;
                int start = starts[vmask[i]];

                // mask 0 means it's a point inside the screen
                if(vmask[i] == 0) {
                    printf("[%d] vmi 0\n", i);
                    inside = true;
                    *q++ = orig.v[i];
                }

                uint8_t local_edge_mask = 0;

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

                    // the point of intersection might be outside the screen on the "other" axis
                    // we don't want to add a point in that case.

                    if(xy <= w && xy >= -w) {

                        // we just insersected, so our inside/outside-ness swaps
                        inside = !inside;

                        printf("add v %x\n", e);

                        // record which screen-edges this triangl-edge intersects
                        local_edge_mask |= (1 << e);

                        // add the point to the output
                        *q++ = p;

                        // if we're exiting the screen, record this info in exit_index
                        if(!inside) {
                            exit_index[e] = (q - points);
                        }
                    }
                }

                // update the overall screen edge intersection state with xor
                edge_mask ^= local_edge_mask;
            }

            if(edge_mask) {
                if((edge_mask == 0xa || edge_mask == 0x5)) {
                    printf("TODO: | - corner case\n");

                    // we need to add 2 adjacent corners
                    //
                    //  a --- b
                    //  |     |
                    //  c --- d
                    //
                    // for mask 0xa, either a+c or b+d
                    // for mask 0x5, either a+b or c+d
                    //
                    // which pair depends on if any point in the triangle is to the left/right (for 0xa) or up/down (for 0x5) of the screen.
                    // TODO: track this info

                } else {
                    // add single corner

                    assert((edge_mask % 3) == 0);

                    //printf("indices: %d %d %d %d\n", exit_index[0], exit_index[1], exit_index[2], exit_index[3]);

                    int exit_i = exit_index[LOG2(edge_mask & 0x5)] | exit_index[LOG2(edge_mask & 0xa)];

                    printf("adding corner %x %d %zd\n", edge_mask, exit_i, (q - points));

                    // shift existing points to make room for the new one
                    memmove(points + exit_i + 1, points + exit_i, sizeof(v4) * ((q - points) - exit_i));

                    // (* 11 & 7) is a trick for doing (/ 3) without integer division.
                    points[exit_i] = corners[((edge_mask * 11) & 7) - 1];
                    ++q;
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
