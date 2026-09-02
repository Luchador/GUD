#define _GNU_SOURCE
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libpdtex/pdtex.h"

#define RAW_TEXTURE_MAGIC "GUTX"
#define RAW_TEXTURE_DESC_OFFSET 16
#define RAW_TEXTURE_DESC_SIZE 12
#define RAW_TEXTURE_HEADER_SIZE (RAW_TEXTURE_DESC_OFFSET + PDTEX_MAX_IMAGES * RAW_TEXTURE_DESC_SIZE)

static uint32_t align_u32(uint32_t value, uint32_t alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

static void write_be16(uint8_t *dst, uint16_t value)
{
	dst[0] = value >> 8;
	dst[1] = value;
}

static void write_be32(uint8_t *dst, uint32_t value)
{
	dst[0] = value >> 24;
	dst[1] = value >> 16;
	dst[2] = value >> 8;
	dst[3] = value;
}

static int source_row_size(const struct pd_image *image)
{
	switch (image->format) {
	case PDFORMAT_RGBA32: return image->width * 4;
	case PDFORMAT_RGB24: return image->width * 3;
	case PDFORMAT_RGBA16:
	case PDFORMAT_RGB15:
	case PDFORMAT_IA16: return image->width * 2;
	case PDFORMAT_IA4: return (image->width + 1) / 2;
	default: return image->width;
	}
}

static int output_row_size(const struct pd_image *image)
{
	switch (image->format) {
	case PDFORMAT_RGBA32:
	case PDFORMAT_RGB24:
		return align_u32(image->width, 4) * 4;
	case PDFORMAT_RGBA16:
	case PDFORMAT_RGB15:
	case PDFORMAT_IA16:
		return align_u32(image->width, 4) * 2;
	case PDFORMAT_IA8:
	case PDFORMAT_I8:
	case PDFORMAT_RGBA16_CI8:
	case PDFORMAT_IA16_CI8:
		return align_u32(image->width, 8);
	case PDFORMAT_IA4:
	case PDFORMAT_I4:
	case PDFORMAT_RGBA16_CI4:
	case PDFORMAT_IA4_CI4:
		return align_u32(image->width, 16) / 2;
	}

	return 0;
}

static uint8_t *make_rdp_pixels(const struct pd_image *image, uint32_t *size)
{
	int source_stride = source_row_size(image);
	int output_stride = output_row_size(image);
	uint8_t *result;
	int x;
	int y;

	if (output_stride == 0) {
		return NULL;
	}

	*size = output_stride * image->height;
	result = calloc(1, *size);

	if (!result) {
		return NULL;
	}

	for (y = 0; y < image->height; y++) {
		const uint8_t *src = image->pixels + y * source_stride;
		uint8_t *dst = result + y * output_stride;

		switch (image->format) {
		case PDFORMAT_RGB24:
			for (x = 0; x < image->width; x++) {
				dst[x * 4 + 0] = src[x * 3 + 0];
				dst[x * 4 + 1] = src[x * 3 + 1];
				dst[x * 4 + 2] = src[x * 3 + 2];
				dst[x * 4 + 3] = image->compression == PDCOMPRESSION_LOOKUP ? 0 : 0xff;
			}
			break;
		case PDFORMAT_I4:
		case PDFORMAT_RGBA16_CI4:
		case PDFORMAT_IA4_CI4:
			for (x = 0; x < image->width; x++) {
				if ((x & 1) == 0) {
					dst[x >> 1] = (src[x] & 0xf) << 4;
				} else {
					dst[x >> 1] |= src[x] & 0xf;
				}
			}
			break;
		default:
			memcpy(dst, src, source_stride);
			break;
		}
	}

	return result;
}

static int write_padding(FILE *fp, uint32_t count)
{
	static const uint8_t zeros[16] = {0};

	while (count > 0) {
		uint32_t amount = count > sizeof(zeros) ? sizeof(zeros) : count;
		if (fwrite(zeros, amount, 1, fp) != 1) {
			return 0;
		}
		count -= amount;
	}

	return 1;
}

static int write_raw_texture(FILE *fp, struct pd_tex *texture, uint32_t *record_size)
{
	uint8_t header[640] = {0};
	uint32_t record_start = ftell(fp);
	uint32_t header_size = align_u32(RAW_TEXTURE_HEADER_SIZE + texture->numcolours * 2, 16);
	uint32_t current_size;
	int image_count = texture->num_images;
	int i;

	if (header_size > sizeof(header) || image_count > PDTEX_MAX_IMAGES) {
		return 0;
	}

	memcpy(header, RAW_TEXTURE_MAGIC, 4);
	header[4] = texture->has_explicit_lods ? 1 : 0;
	header[5] = texture->lod_count;
	header[6] = image_count;
	write_be16(&header[8], texture->numcolours);
	write_be16(&header[10], header_size);

	if (texture->palette) {
		memcpy(&header[RAW_TEXTURE_HEADER_SIZE], texture->palette, texture->numcolours * 2);
	}

	if (!write_padding(fp, header_size)) {
		return 0;
	}

	for (i = 0; i < image_count; i++) {
		struct pd_image *image = &texture->images[i];
		uint8_t *descriptor = &header[RAW_TEXTURE_DESC_OFFSET + i * RAW_TEXTURE_DESC_SIZE];
		uint8_t *pixels;
		uint32_t data_size;
		uint32_t data_offset = ftell(fp) - record_start;

		if (!image->exists) {
			return 0;
		}

		pixels = make_rdp_pixels(image, &data_size);
		if (!pixels) {
			return 0;
		}

		descriptor[0] = image->format;
		descriptor[1] = image->width;
		descriptor[2] = image->height;
		write_be32(&descriptor[4], data_offset);
		write_be32(&descriptor[8], data_size);

		if (fwrite(pixels, data_size, 1, fp) != 1) {
			free(pixels);
			return 0;
		}

		free(pixels);
	}

	current_size = ftell(fp) - record_start;

	if (!write_padding(fp, align_u32(current_size, 16) - current_size)) {
		return 0;
	}

	*record_size = ftell(fp) - record_start;
	write_be32(&header[12], *record_size);

	if (fseek(fp, record_start, SEEK_SET) != 0 || fwrite(header, header_size, 1, fp) != 1) {
		return 0;
	}

	return fseek(fp, record_start + *record_size, SEEK_SET) == 0;
}

static int write_raw_def(const char *source_path, const char *output_path,
		const uint32_t *sizes, size_t size_count)
{
	FILE *source = fopen(source_path, "r");
	FILE *output = fopen(output_path, "w");
	char *line = NULL;
	size_t capacity = 0;
	size_t index = 0;

	if (!source || !output) {
		return 0;
	}

	while (getline(&line, &capacity, source) >= 0) {
		char *first_comma;
		char *second_comma;

		if (strncmp(line, "IMAGE(", 6) != 0) {
			fputs(line, output);
			continue;
		}

		if (index >= size_count) {
			free(line);
			fclose(source);
			fclose(output);
			return 0;
		}

		first_comma = strchr(line, ',');
		second_comma = first_comma ? strchr(first_comma + 1, ',') : NULL;

		if (!second_comma) {
			free(line);
			fclose(source);
			fclose(output);
			return 0;
		}

		fprintf(output, "%.*s 0x%X%s", (int)(first_comma - line + 1), line,
				sizes[index++], second_comma);
	}

	free(line);
	fclose(source);
	fclose(output);
	return index == size_count;
}

static int usage(const char *program)
{
	fprintf(stderr, "Usage: %s <imagelist.csv> <images.def> <combined.bin> <raw.def>\n", program);
	return 1;
}

int main(int argc, char **argv)
{
	FILE *list;
	FILE *combined;
	char *line = NULL;
	size_t capacity = 0;
	size_t count = 0;
	size_t sizes_capacity = 0;
	uint32_t *sizes = NULL;
	int trace = getenv("TEX2RAW_TRACE") != NULL;
	int result = 1;

	if (argc != 5) {
		return usage(argv[0]);
	}

	list = fopen(argv[1], "r");
	combined = fopen(argv[3], "wb");

	if (!list || !combined) {
		fprintf(stderr, "tex2raw: %s\n", strerror(errno));
		return 1;
	}

	while (getline(&line, &capacity, list) >= 0) {
		char *first_comma = strchr(line, ',');
		char *second_comma = first_comma ? strchr(first_comma + 1, ',') : NULL;
		char *third_comma = second_comma ? strchr(second_comma + 1, ',') : NULL;
		struct pd_tex *texture;
		uint32_t record_size;

		if (!second_comma || !third_comma) {
			goto done;
		}

		*third_comma = '\0';

		if (trace) {
			fprintf(stderr, "tex2raw: decoding [%zu] %s\n", count, second_comma + 1);
			fflush(stderr);
		}

		texture = pdtex_allocate();

		if (!texture || pdtex_read_native(texture, second_comma + 1) != 0) {
			fprintf(stderr, "tex2raw: failed to decode %s\n", second_comma + 1);
			pdtex_free(texture);
			goto done;
		}

		if (count == sizes_capacity) {
			size_t new_capacity = sizes_capacity ? sizes_capacity * 2 : 256;
			uint32_t *new_sizes = realloc(sizes, new_capacity * sizeof(*sizes));
			if (!new_sizes) {
				pdtex_free(texture);
				goto done;
			}
			sizes = new_sizes;
			sizes_capacity = new_capacity;
		}

		if (!write_raw_texture(combined, texture, &record_size)) {
			fprintf(stderr, "tex2raw: failed to convert %s\n", second_comma + 1);
			pdtex_free(texture);
			goto done;
		}

		sizes[count++] = record_size;
		pdtex_free(texture);
	}

	if (fclose(combined) != 0) {
		combined = NULL;
		goto done;
	}
	combined = NULL;

	if (!write_raw_def(argv[2], argv[4], sizes, count)) {
		fprintf(stderr, "tex2raw: failed to write %s\n", argv[4]);
		goto done;
	}

	result = 0;

done:
	free(line);
	free(sizes);
	fclose(list);
	if (combined) {
		fclose(combined);
	}
	return result;
}
