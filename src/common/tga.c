#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include "halcyon.h"

struct tga_header  {
	uint8_t  id_len;
	uint8_t  has_cmap;
	uint8_t  img_type;
	uint16_t cmap_off;
	uint16_t cmap_len;
	uint8_t  cmap_bpp;
	uint16_t xorigin;
	uint16_t yorigin;
	uint16_t width;
	uint16_t height;
	uint8_t  bpp;
	uint8_t  attr;
} __attribute__((packed));

bool tga_load(const void* data, size_t len, hc_tex* tex){
	const struct tga_header* hdr = data;

	assert(len > sizeof(*hdr));

	assert(hdr->has_cmap == 1); // has colormap
	assert(hdr->img_type == 9); // colormapped, RLE'd

	const uint8_t* ptr = data + sizeof(*hdr) + hdr->id_len;
	const uint8_t* end = data + len;

	assert(ptr < end);

	const uint8_t bytes_per_cmap_entry = (hdr->cmap_bpp + 7) / 8;
	const uint8_t bytes_per_pixel = (hdr->bpp + 7) / 8;

	const uint8_t* cmap = hdr->cmap_len ? ptr : NULL;
	ptr += (bytes_per_cmap_entry * hdr->cmap_len);

	tex->w = hdr->width;
	tex->h = hdr->height;

	const uint32_t size = hdr->width * hdr->height;
	uint8_t* out = tex->data = calloc(size, bytes_per_cmap_entry);
	const uint8_t* out_end = out + size * bytes_per_cmap_entry;

	assert(out);

	if(cmap){
		const uint8_t* cmap_end = cmap + hdr->cmap_len * bytes_per_cmap_entry;

		while(ptr < end && out < out_end){
			uint8_t id = *ptr++;

			uint8_t reps;
			uint8_t count;

			if(id & 0x80){ // RLE
				count = 1;
				reps = (id & 0x7f) + 1;
			} else {
				reps = 1;
				count = (id & 0x7f) + 1;
			}

			for(int c = 0; c < count; ++c){
				uint32_t cmap_index = 0;
				for(int i = 0; i < bytes_per_pixel; ++i){
					cmap_index <<= 8;
					cmap_index |= *ptr++;

					assert(ptr <= end);
				}
				cmap_index -= hdr->cmap_off;

				assert(cmap + cmap_index * bytes_per_cmap_entry < cmap_end);

				for(int r = 0; r < reps; ++r){
					static const int rgba_shuffle[] = { 2, 1, 0, 3 };

					for(int i = 0; i < bytes_per_cmap_entry; ++i){
						int j = rgba_shuffle[i];

						*out++ = cmap[cmap_index * bytes_per_cmap_entry + j];
						assert(out - (uint8_t*)tex->data <= size * bytes_per_cmap_entry);
					}
				}
			}
		}
	} else {
		assert(0);
	}

	return true;
}
