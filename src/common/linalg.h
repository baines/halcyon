#ifndef LINALG_H_
#define LINALG_H_
#include <math.h>
#include <string.h>
#include <stdio.h>

typedef float v2 __attribute__((vector_size(8)));
typedef float v3[3];
typedef float v4 __attribute__((vector_size(16)));

typedef int v4i __attribute__((vector_size(16)));

// [along][down]
typedef v4 mat4[4];

#define MAT4_ID {{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}}

static inline float v4_dot(v4 a, v4 b){
	static const v4i mask1 = { 1, 0, 3, 2};
	static const v4i mask2 = { 3, 2, 1, 0};

	a = a * b;
	b = __builtin_shuffle(a, mask1);
	a = a + b;
	b = __builtin_shuffle(a, mask2);
	return (a + b)[0];
}

static inline float v4_len(v4 a){
	return sqrt(v4_dot(a, a));
}

static inline v4 v4_norm(v4 a){
	return a / v4_len(a);
}

static inline void mat4_mul(mat4 dst, const mat4 a, const mat4 b){
    for(int i = 0; i < 4; ++i){
        v4 x = { a[i][0], a[i][0], a[i][0], a[i][0] };
        v4 y = { a[i][1], a[i][1], a[i][1], a[i][1] };
        v4 z = { a[i][2], a[i][2], a[i][2], a[i][2] };
        v4 w = { a[i][3], a[i][3], a[i][3], a[i][3] };

        dst[i] = b[0] * x + b[1] * y + b[2] * z + b[3] * w;
    }
}

static inline v4 mat4_dp4(mat4 a, v4 b){
	return (v4){
		v4_dot(a[0], b),
		v4_dot(a[1], b),
		v4_dot(a[2], b),
		v4_dot(a[3], b),
	};
}

static inline void mat4_id(mat4 m){
	memset(m, 0, 16*4);
	m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0f;
}

static inline void mat4_transpose(mat4 m){
	v4 t0 = __builtin_ia32_unpcklps(m[0], m[1]);
	v4 t2 = __builtin_ia32_unpcklps(m[2], m[3]);
	v4 t1 = __builtin_ia32_unpckhps(m[0], m[1]);
	v4 t3 = __builtin_ia32_unpckhps(m[2], m[3]);

	m[0] = __builtin_ia32_movlhps(t0, t2);
	m[1] = __builtin_ia32_movhlps(t2, t0);
	m[2] = __builtin_ia32_movlhps(t1, t3);
	m[3] = __builtin_ia32_movhlps(t3, t1);
}

static inline void ortho(mat4 m, float l, float r, float t, float b, float n, float f){
	memset(m, 0, sizeof(mat4));

	m[0][0] =  2.0f / (r - l);
	m[1][1] =  2.0f / (t - b);
	m[2][2] = -2.0f / (f - n);
	m[3][3] =  1.0f;

	m[3][0] = -(r + l) / (r - l);
	m[3][1] = -(t + b) / (t - b);
	m[3][2] = -(f + n) / (f - n);
}

static inline void frustum(mat4 m, float l, float r, float b, float t, float n, float f){
	memset(m, 0, sizeof(mat4));

	m[0][0] = (2.0f * n) / (r - l);
	m[1][1] = (2.0f * n) / (t - b);

	m[0][2] =  (r + l) / (r - l);
	m[1][2] =  (t + b) / (t - b);
	m[2][2] = -(f + n) / (f - n);
	m[3][2] = -1.0f;

	m[2][3] = (-2.0f * n * f) / (f - n);
}

void mat4_print(mat4 m){
	for(int i = 0; i < 4; ++i){
		printf("{ ");
		for(int j = 0; j < 4; ++j){
			printf("%+3.2f, ", m[i][j]);
		}
		printf("},\n");
	}
}

void v4_print(v4 v){
	printf("[ ");
	for(int i = 0; i < 4; ++i){
		printf("%+3.2f, ", v[i]);
	}
	printf("]\n");
}

v4 cross(v4 a, v4 b){
	return (v4){
		a[1] * b[2] - a[2] * b[1],
		a[2] * b[0] - a[0] * b[2],
		a[0] * b[1] - a[1] * b[0],
	};
}

#endif
