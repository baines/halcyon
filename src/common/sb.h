#ifndef SB_H_
#define SB_H_
#include <stddef.h>
#include <stdlib.h>

struct sb_header {
	size_t capacity;
	size_t used;
} __attribute__((aligned(16)));

#define sb(type)       type*
#define sb_each(p, s)  for(typeof(s) p = s; p != sb_end(s); ++p)

#define sb_push(s, v)  (__sb_alloc(&(s), sizeof(*s), 1), (s)[__sbh(s)->used++] = (v))
#define sb_pop(s)      (__sbh(s)->used--)
#define sb_add(s, n)   (__sb_alloc(&(s), sizeof(*s), (n)), __sbh(s)->used+=(n), &(s)[__sbh(s)->used-(n)])
#define sb_count(s)    ((s) ? __sbh(s)->used : 0)
#define sb_last(s)     ((s)[__sbh(s)->used-1])
#define sb_end(s)      ((s) ? (s) + __sbh(s)->used : 0)
#define sb_free(s)     ((s) ? free(__sbh(s)),(s)=0,0 : 0)
#define sb_erase(s, i) ((s) ? memmove((s)+(i), (s)+(i)+1, (--__sbh(s)->used-(i))*sizeof(*(s))) : 0)

#define __sbh(s) ((struct sb_header*)(((char*)(s))-sizeof(struct sb_header)))

static inline void __sb_alloc(void* addr, size_t size, size_t nmemb){
	struct sb_header h = {}, *ptr = NULL;

	if(*(void**)addr){
		ptr = __sbh(*(void**)addr);
		h   = *ptr;
	}

	if((h.used + nmemb) * size >= h.capacity){
		nmemb = h.capacity ? nmemb : 8;
		h.capacity += (h.capacity >> 1) + (nmemb * size);

		ptr = realloc(ptr, h.capacity + sizeof(h));
		if(!ptr){
			perror("__sb_alloc");
			abort();
		}

		*ptr = h;
		*(void**)addr = ptr+1;
	}
}

#endif
