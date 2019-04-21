#include "halcyon.h"
#include <stdlib.h>

int main(void){
	hc_init(NULL);

	for(;;){
		hc_v2 a = { rand() % WIN_W, rand() % WIN_H };
		hc_v2 b = { rand() % WIN_W, rand() % WIN_H };

		hc_line(a, b, rand());

		hc_finish(0);
	}
}
