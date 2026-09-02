#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>
#include "pdtex.h"

#define ZLIB_TEXTURE_MAX_PIXELS 0x1000
#define NON_ZLIB_TEXTURE_MAX_PIXELS 0x2000

/**
 * The functions in this file are copied from PD decomp's texdecompress.c.
 *
 * Refer to decomp for function documentation.
 */

uint8_t *var800ab540;
uint32_t var800ab544;
int var800ab548;
int total_read;
void *g_RzipUnused;

extern int g_TexFormatNumChannels[];
extern int g_TexFormatHas1BitAlpha[];
extern int g_TexFormatChannelSizes[];
extern int g_TexFormatBitsPerPixel[];

void pdtex_flip(struct pd_tex *tex);

int rzipInflate(uint8_t *src, uint8_t *dst, uint32_t output_size)
{
	int ret;
	z_stream strm;

	if (src[0] == 0x11 && src[1] == 0x73) {
		src += 5;
	} else if (src[0] == 0x11 && src[1] == 0x72) {
		src += 2;
	} else {
		fprintf(stderr, "rzipInflate: Not 1172 or 1173\n");
		fprintf(stderr, "%02x ", src[0]);
		fprintf(stderr, "%02x ", src[1]);
		fprintf(stderr, "%02x ", src[2]);
		fprintf(stderr, "%02x ", src[3]);
		fprintf(stderr, "%02x ", src[4]);
		fprintf(stderr, "%02x ", src[5]);
		fprintf(stderr, "%02x ", src[6]);
		fprintf(stderr, "%02x ", src[7]);
		fprintf(stderr, "\n");
		return Z_STREAM_ERROR;
	}

	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	ret = inflateInit2(&strm, -15);

	if (ret != Z_OK) {
		return ret;
	}

	strm.avail_in = 0x2000;
	strm.next_in = src;
	strm.avail_out = output_size;
	strm.next_out = dst;

	ret = inflate(&strm, Z_FINISH);

	if (ret != Z_STREAM_END) {
		inflateEnd(&strm);
		return ret == Z_OK ? Z_BUF_ERROR : ret;
	}

	g_RzipUnused = strm.next_in;

	inflateEnd(&strm);

	return Z_OK;
}

void *rzipGetUnused(void)
{
	return g_RzipUnused;
}

static void texSetBitstring(uint8_t *arg0)
{
	var800ab540 = arg0;
	var800ab544 = 0;
	var800ab548 = 0;
	total_read = 0;
}

static int texReadBits(int numbits)
{
	uint32_t mask;

	while (var800ab548 < numbits) {
		var800ab544 = *var800ab540 | var800ab544 << 8;
		var800ab540++;
		var800ab548 += 8;
	}

	total_read += numbits;
	var800ab548 -= numbits;
	mask = numbits == 0 ? 0 : UINT32_MAX >> (32 - numbits);

	return var800ab544 >> var800ab548 & mask;
}

static void texInflateHuffman(uint8_t *dst, int numiterations, int chansize)
{
	uint16_t frequencies[2048];
	int16_t nodes[2048][2];
	int32_t i;
	int32_t rootindex;
	int32_t sum;
	uint16_t minfreq1;
	uint16_t minfreq2;
	int32_t minindex1 = 0;
	int32_t minindex2 = 1;
	bool done = false;

	// Read the frequencies list
	for (i = 0; i < chansize; i++) {
		frequencies[i] = texReadBits(8);
	}

	// A one-symbol alphabet needs no tree or encoded index bits.
	if (chansize == 1) {
		memset(dst, 0, numiterations);
		return;
	}

	// Initialise the tree
	for (i = 0; i < 2048; i++) {
		nodes[i][0] = -1;
		nodes[i][1] = -1;
	}

	// Find the two smallest frequencies
	minfreq1 = 9999;
	minfreq2 = 9999;

	for (i = 0; i < chansize; i++) {
		if (frequencies[i] < minfreq1) {
			if (minfreq2 < minfreq1) {
				minfreq1 = frequencies[i];
				minindex1 = i;
			} else {
				minfreq2 = frequencies[i];
				minindex2 = i;
			}
		} else if (frequencies[i] < minfreq2) {
			minfreq2 = frequencies[i];
			minindex2 = i;
		}
	}

	// Build the tree.
	// For each node in tree, a branch value < 10000 means this branch
	// leads to another node, and the value is the target node's index.
	// A branch value >= 10000 means the branch is a leaf node,
	// and the value is the channel value + 10000.
	while (!done) {
		sum = frequencies[minindex1] + frequencies[minindex2];

		if (sum == 0) {
			sum = 1;
		}

		frequencies[minindex1] = 9999;
		frequencies[minindex2] = 9999;

		if (nodes[minindex1][0] < 0 && nodes[minindex1][1] < 0) {
			nodes[minindex1][0] = minindex1 + 10000;
			rootindex = minindex1;
			frequencies[minindex1] = sum;

			if (nodes[minindex2][0] < 0 && nodes[minindex2][1] < 0) {
				nodes[minindex1][1] = minindex2 + 10000;
			} else {
				nodes[minindex1][1] = minindex2;
			}
		} else if (nodes[minindex2][0] < 0 && nodes[minindex2][1] < 0) {
			nodes[minindex2][0] = minindex2 + 10000;
			rootindex = minindex2;
			frequencies[minindex2] = sum;

			if (nodes[minindex1][0] < 0 && nodes[minindex1][1] < 0) {
				nodes[minindex2][1] = minindex1 + 10000;
			} else {
				nodes[minindex2][1] = minindex1;
			}
		} else {
			for (rootindex = 0; nodes[rootindex][0] >= 0 || nodes[rootindex][1] >= 0 || frequencies[rootindex] < 9999; rootindex++);

			frequencies[rootindex] = sum;
			nodes[rootindex][0] = minindex1;
			nodes[rootindex][1] = minindex2;
		}

		// Find the two smallest frequencies again for the next iteration
		minfreq1 = 9999;
		minfreq2 = 9999;

		for (i = 0; i < chansize; i++) {
			if (frequencies[i] < minfreq1) {
				if (minfreq1 > minfreq2) {
					minfreq1 = frequencies[i];
					minindex1 = i;
				} else {
					minfreq2 = frequencies[i];
					minindex2 = i;
				}
			} else if (frequencies[i] < minfreq2) {
				minfreq2 = frequencies[i];
				minindex2 = i;
			}
		}

		if (minfreq1 == 9999 || minfreq2 == 9999) {
			done = true;
		}
	}

	// Read bits off the bitstring, traverse the tree
	// and write the channel values to dst
	for (i = 0; i < numiterations; i++) {
		int indexorvalue = rootindex;

		while (indexorvalue < 10000) {
			indexorvalue = nodes[indexorvalue][texReadBits(1)];
		}

		if (chansize <= 256) {
			dst[i] = indexorvalue - 10000;
		} else {
			int value = indexorvalue - 10000;
			dst[i * 2 + 0] = value >> 8;
			dst[i * 2 + 1] = value & 0xff;
		}
	}
}

static void texInflateRle(uint8_t *dst, int blockstotal)
{
	int btfieldsize = texReadBits(3);
	int rlfieldsize = texReadBits(3);
	int blocksize = texReadBits(4);
	int cost;
	int fudge;
	int blocksdone;
	int i;

	// Calculate the fudge value
	cost = btfieldsize + rlfieldsize + blocksize + 1;
	fudge = 0;

	while (cost > 0) {
		cost = cost - blocksize - 1;
		fudge++;
	}

	blocksdone = 0;

	while (blocksdone < blockstotal) {
		if (texReadBits(1) == 0) {
			// Found a literal directive
			if (blocksize <= 8) {
				dst[blocksdone] = texReadBits(blocksize);
				blocksdone++;
			} else {
				uint16_t *tmp = (uint16_t *)dst;
				tmp[blocksdone] = texReadBits(blocksize);
				blocksdone++;
			}
		} else {
			// Found a run directive
			int startblockindex = blocksdone - texReadBits(btfieldsize) - 1;
			int runnumblocks = texReadBits(rlfieldsize) + fudge;

			if (blocksize <= 8) {
				for (i = startblockindex; i < startblockindex + runnumblocks; i++) {
					dst[blocksdone] = dst[i];
					blocksdone++;
				}

				// The next instruction must be a literal
				dst[blocksdone] = texReadBits(blocksize);
				blocksdone++;
			} else {
				uint16_t *tmp = (uint16_t *)dst;

				for (i = startblockindex; i < startblockindex + runnumblocks; i++) {
					tmp[blocksdone] = tmp[i];
					blocksdone++;
				}

				// The next instruction must be a literal
				tmp[blocksdone] = texReadBits(blocksize);
				blocksdone++;
			}
		}
	}
}

static int texBuildLookup(uint8_t *lookup, int bitsperpixel)
{
	int numcolours = texReadBits(11);
	int i;

	if (bitsperpixel <= 16) {
		for (i = 0; i < numcolours; i++) {
			int value = texReadBits(bitsperpixel);
			lookup[i * 2 + 0] = (value >> 8) & 0xff;
			lookup[i * 2 + 1] = value & 0xff;
		}
	} else if (bitsperpixel <= 24) {
		for (i = 0; i < numcolours; i++) {
			int value = texReadBits(bitsperpixel);
			lookup[i * 4 + 0] = (value >> 24);
			lookup[i * 4 + 1] = (value >> 16) & 0xff;
			lookup[i * 4 + 2] = (value >> 8) & 0xff;
			lookup[i * 4 + 3] = value & 0xff;
		}
	} else {
		for (i = 0; i < numcolours; i++) {
			uint32_t value = texReadBits(24) << 8 | texReadBits(bitsperpixel - 24);

			lookup[i * 4 + 0] = (value >> 24);
			lookup[i * 4 + 1] = (value >> 16) & 0xff;
			lookup[i * 4 + 2] = (value >> 8) & 0xff;
			lookup[i * 4 + 3] = value & 0xff;
		}
	}

	return numcolours;
}

static int texGetBitSize(int decimal)
{
	int count = 0;

	decimal--;

	while (decimal > 0) {
		decimal >>= 1;
		count++;
	}

	return count;
}

static void texReadAlphaBits(uint8_t *dst, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		dst[i] = texReadBits(1);
	}
}

static void texReadUncompressed(struct pd_image *image)
{
	int x;
	int y;
	int width = image->width;
	int height = image->height;
	uint8_t *dst = image->pixels;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			switch (image->format) {
			case PDFORMAT_RGBA32:
				dst[x * 4 + 0] = texReadBits(8);
				dst[x * 4 + 1] = texReadBits(8);
				dst[x * 4 + 2] = texReadBits(8);
				dst[x * 4 + 3] = texReadBits(8);
				break;
			case PDFORMAT_RGB24:
				dst[x * 3 + 0] = texReadBits(8);
				dst[x * 3 + 1] = texReadBits(8);
				dst[x * 3 + 2] = texReadBits(8);
				break;
			case PDFORMAT_RGBA16:
			case PDFORMAT_IA16: {
				int value = texReadBits(16);
				dst[x * 2 + 0] = value >> 8;
				dst[x * 2 + 1] = value;
				break;
			}
			case PDFORMAT_RGB15: {
				int value = texReadBits(15) << 1 | 1;
				dst[x * 2 + 0] = value >> 8;
				dst[x * 2 + 1] = value;
				break;
			}
			case PDFORMAT_IA8:
			case PDFORMAT_I8:
				dst[x] = texReadBits(8);
				break;
			case PDFORMAT_IA4: {
				int value = texReadBits(4);
				if ((x & 1) == 0) {
					dst[x >> 1] = value << 4;
				} else {
					dst[x >> 1] |= value;
				}
				break;
			}
			case PDFORMAT_I4:
				dst[x] = texReadBits(4);
				break;
			default:
				break;
			}
		}

		switch (image->format) {
		case PDFORMAT_RGBA32: dst += width * 4; break;
		case PDFORMAT_RGB24: dst += width * 3; break;
		case PDFORMAT_RGBA16:
		case PDFORMAT_RGB15:
		case PDFORMAT_IA16: dst += width * 2; break;
		case PDFORMAT_IA4: dst += (width + 1) / 2; break;
		default: dst += width; break;
		}
	}
}

static void texChannelsToPixels(uint8_t *src, int width, int height, uint8_t *dst, int format)
{
	int x;
	int y;
	int pos = 0;
	int mult = width * height;

	switch (format) {
	case PDFORMAT_RGBA32:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				dst[x * 4 + 0] = src[pos];
				dst[x * 4 + 1] = src[pos + mult];
				dst[x * 4 + 2] = src[pos + mult * 2];
				dst[x * 4 + 3] = src[pos + mult * 3];
				pos++;
			}
			dst += width * 4;
		}

		break;
	case PDFORMAT_RGB24:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				dst[x * 3 + 0] = src[pos];
				dst[x * 3 + 1] = src[pos + mult];
				dst[x * 3 + 2] = src[pos + mult * 2];
				pos++;
			}
			dst += width * 3;
		}

		break;
	case PDFORMAT_RGBA16:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				dst[x * 2 + 0] = src[pos] << 3 | src[pos + mult] >> 2;
				dst[x * 2 + 1] = src[pos + mult] << 6 | src[pos + mult * 2] << 1 | src[pos + mult * 3];
				pos++;
			}
			dst += width * 2;
		}

		break;
	case PDFORMAT_IA16:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				dst[x * 2 + 0] = src[pos];
				dst[x * 2 + 1] = src[pos + mult];
				pos++;
			}
			dst += width * 2;
		}

		break;
	case PDFORMAT_RGB15:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				dst[x * 2 + 0] = src[pos] << 3 | src[pos + mult] >> 2;
				dst[x * 2 + 1] = src[pos + mult] << 6 | src[pos + mult * 2] << 1 | 1;
				pos++;
			}
			dst += width * 2;
		}

		break;
	case PDFORMAT_IA8:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				dst[x] = src[pos] << 4 | src[pos + mult];
				pos++;
			}
			dst += width;
		}

		break;
	case PDFORMAT_I8:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				dst[x] = src[pos];
				pos++;
			}
			dst += width;
		}

		break;
	case PDFORMAT_IA4:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x += 2) {
				dst[x >> 1] = src[pos] << 5 | src[pos + mult * 3] << 4 | src[pos + 1] << 1 | src[pos + mult * 3 + 1];
				pos += 2;
			}
			if (width & 1) {
				pos--;
			}

			dst += (width + 1) / 2;
		}

		break;
	case PDFORMAT_I4:
		memcpy(dst, src, width * height);
		break;
	}
}

static void texInflateLookup(int width, int height, uint8_t *dst, uint8_t *lookup, int numcolours, int format)
{
	int x;
	int y;
	int bitspercolour = texGetBitSize(numcolours);

	switch (format) {
	case PDFORMAT_RGBA32:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				int index = texReadBits(bitspercolour);
				dst[x * 4 + 0] = lookup[index * 4 + 0];
				dst[x * 4 + 1] = lookup[index * 4 + 1];
				dst[x * 4 + 2] = lookup[index * 4 + 2];
				dst[x * 4 + 3] = lookup[index * 4 + 3];
			}

			dst += width * 4;
		}

		break;
	case PDFORMAT_RGB24:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				int index = texReadBits(bitspercolour);
				dst[x * 3 + 0] = lookup[index * 4 + 1];
				dst[x * 3 + 1] = lookup[index * 4 + 2];
				dst[x * 3 + 2] = lookup[index * 4 + 3];
			}

			dst += width * 3;
		}

		break;
	case PDFORMAT_RGBA16:
	case PDFORMAT_IA16:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				int index = texReadBits(bitspercolour);
				dst[x * 2 + 0] = lookup[index * 2 + 0];
				dst[x * 2 + 1] = lookup[index * 2 + 1];
			}

			dst += width * 2;
		}

		break;
	case PDFORMAT_RGB15:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				int index = texReadBits(bitspercolour);
				dst[x * 2 + 0] = lookup[index * 2 + 0] << 1 | ((lookup[index * 2 + 1] >> 7) & 1);
				dst[x * 2 + 1] = lookup[index * 2 + 1] << 1 | 1;
			}

			dst += width * 2;
		}

		break;
	case PDFORMAT_IA8:
	case PDFORMAT_I8:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				dst[x] = lookup[texReadBits(bitspercolour) * 2 + 1];
			}

			dst += width;
		}

		break;
	case PDFORMAT_IA4:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x += 2) {
				dst[x >> 1] = lookup[texReadBits(bitspercolour) * 2 + 1] << 4;

				if (x + 1 < width) {
					dst[x >> 1] |= lookup[(texReadBits(bitspercolour) * 2) + 1];
				}
			}

			dst += (width + 1) >> 1;
		}

		break;
	case PDFORMAT_I4:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				dst[x] = lookup[texReadBits(bitspercolour) * 2 + 1] & 0xf;
			}

			dst += width;
		}

		break;
	}
}

static void texInflateLookupFromBuffer(uint8_t *src, int width, int height, uint8_t *dst, uint8_t *lookup, int numcolours, int format)
{
	int x;
	int y;
	uint16_t indexesarray[0x2000];
	uint16_t *indexes = indexesarray;

	if (numcolours <= 256) {
		for (int i = 0; i < width * height; i++) {
			indexesarray[i] = src[i];
		}
	} else {
		for (int i = 0; i < width * height; i++) {
			indexesarray[i] = src[i * 2] << 8 | src[i * 2 + 1];
		}
	}

	switch (format) {
	case PDFORMAT_RGBA32:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				dst[x * 4 + 0] = lookup[indexes[x] * 4 + 0];
				dst[x * 4 + 1] = lookup[indexes[x] * 4 + 1];
				dst[x * 4 + 2] = lookup[indexes[x] * 4 + 2];
				dst[x * 4 + 3] = lookup[indexes[x] * 4 + 3];
			}

			dst += width * 4;
			indexes += width;
		}

		break;
	case PDFORMAT_RGB24:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				dst[x * 3 + 0] = lookup[indexes[x] * 4 + 1];
				dst[x * 3 + 1] = lookup[indexes[x] * 4 + 2];
				dst[x * 3 + 2] = lookup[indexes[x] * 4 + 3];
			}

			dst += width * 3;
			indexes += width;
		}

		break;
	case PDFORMAT_RGBA16:
	case PDFORMAT_IA16:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				dst[x * 2 + 0] = lookup[indexes[x] * 2 + 0];
				dst[x * 2 + 1] = lookup[indexes[x] * 2 + 1];
			}

			dst += width * 2;
			indexes += width;
		}

		break;
	case PDFORMAT_RGB15:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				dst[x * 2 + 0] = lookup[indexes[x] * 2] << 1 | (lookup[indexes[x] * 2 + 1] >> 7);
				dst[x * 2 + 1] = lookup[indexes[x] * 2 + 1] << 1 | 1;
			}

			dst += width * 2;
			indexes += width;
		}

		break;
	case PDFORMAT_IA8:
	case PDFORMAT_I8:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				dst[x] = lookup[indexes[x] * 2 + 1];
			}

			dst += width;
			indexes += width;
		}

		break;
	case PDFORMAT_IA4:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x += 2) {
				dst[x >> 1] = lookup[indexes[x] * 2 + 1] << 4;

				if (x + 1 < width) {
					dst[x >> 1] |= lookup[indexes[x + 1] * 2 + 1];
				}
			}

			dst += (width + 1) >> 1;
			indexes += width;
		}

		break;
	case PDFORMAT_I4:
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				dst[x] = lookup[indexes[x] * 2 + 1] & 0xf;
			}

			dst += width;
			indexes += width;
		}

		break;
	}
}

static void texBlur(uint8_t *pixels, int width, int height, int method, int chansize)
{
	int x;
	int y;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			int cur = pixels[y * width + x] + chansize * 2;
			int left = x > 0 ? pixels[y * width + x - 1] : 0;
			int above = y > 0 ? pixels[(y - 1) * width + x] : 0;
			int aboveleft = x > 0 && y > 0 ? pixels[(y - 1) * width + x - 1] : 0;

			switch (method) {
			case 0:
				pixels[y * width + x] = (cur + left) % chansize;
				break;
			case 1:
				pixels[y * width + x] = (cur + above) % chansize;
				break;
			case 2:
				pixels[y * width + x] = (cur + aboveleft) % chansize;
				break;
			case 3:
				pixels[y * width + x] = (cur + (left + above - aboveleft)) % chansize;
				break;
			case 4:
				pixels[y * width + x] = (cur + ((above - aboveleft) / 2 + left)) % chansize;
				break;
			case 5:
				pixels[y * width + x] = (cur + ((left - aboveleft) / 2 + above)) % chansize;
				break;
			case 6:
				pixels[y * width + x] = (cur + ((left + above) / 2)) % chansize;
				break;
			}
		}
	}
}

static void texAlignIndices(uint8_t *src, int width, int height, int format, uint8_t *dst)
{
	int x;
	int y;

	if (format == PDFORMAT_RGBA16_CI8 || format == PDFORMAT_IA16_CI8) {
		// 1 index per byte -> copy it
		memcpy(dst, src, width * height);
	} else if (format == PDFORMAT_RGBA16_CI4 || format == PDFORMAT_IA4_CI4) {
		// 2 indexes per byte -> convert it to 1 index per byte
		for (y = 0; y < height; y++) {
			int readside = 0;

			for (x = 0; x < width; x++) {
				int value = readside ? src[x / 2] & 0xf : src[x / 2] >> 4;
				*dst = value;
				dst++;

				readside = 1 - readside;
			}

			src += (width + 1) / 2;
		}
	}
}

static int texInflateZlib(struct pd_tex *tex, int arg2, int forcenumimages)
{
	int numimages;
	int format;

	if (arg2 && forcenumimages) {
		numimages = forcenumimages;
	} else {
		numimages = 1;
	}

	format = texReadBits(8);

	if (format != PDFORMAT_RGBA16_CI8
			&& format != PDFORMAT_RGBA16_CI4
			&& format != PDFORMAT_IA16_CI8
			&& format != PDFORMAT_IA4_CI4) {
		return 0;
	}

	tex->numcolours = texReadBits(8) + 1;
	tex->palette = malloc(tex->numcolours * 2);

	if (!tex->palette) {
		return 0;
	}

	for (int i = 0; i < tex->numcolours; i++) {
		tex->palette[i * 2 + 0] = texReadBits(8);
		tex->palette[i * 2 + 1] = texReadBits(8);
	}

	for (int i = 0; i < numimages; i++) {
		struct pd_image *image = &tex->images[i];
		uint8_t *scratch;
		uint32_t pixel_count;
		uint32_t index_data_size;

		image->exists = true;
		image->format = format;
		image->width = texReadBits(8);
		image->height = texReadBits(8);
		pixel_count = image->width * image->height;

		// Match the size limit enforced by the runtime texture loader.
		if (pixel_count == 0 || pixel_count > ZLIB_TEXTURE_MAX_PIXELS) {
			return 0;
		}

		if (format == PDFORMAT_RGBA16_CI4 || format == PDFORMAT_IA4_CI4) {
			index_data_size = ((image->width + 1) / 2) * image->height;
		} else {
			index_data_size = pixel_count;
		}

		image->pixels = malloc(pixel_count);
		scratch = malloc(index_data_size);

		if (!image->pixels || !scratch) {
			free(scratch);
			return 0;
		}

		if (rzipInflate(var800ab540, scratch, index_data_size) != Z_OK) {
			free(scratch);
			return 0;
		}

		texAlignIndices(scratch, image->width, image->height, image->format, image->pixels);
		free(scratch);
		texSetBitstring(rzipGetUnused());
	}

	return 1;
}

static int texInflateNonZlib(struct pd_tex *tex, int arg2, int forcenumimages)
{
	uint8_t scratch[0x20000];
	uint8_t lookup[0x10000];
	int numimages;
	int value;

	numimages = arg2 && forcenumimages ? forcenumimages : 1;

	for (int i = 0; i < numimages; i++) {
		struct pd_image *image = &tex->images[i];

		image->exists = true;
		image->format = texReadBits(4);
		image->width = texReadBits(8);
		image->height = texReadBits(8);
		image->compression = texReadBits(4);

		if (image->format > PDFORMAT_IA4_CI4
				|| image->compression > PDCOMPRESSION_RLEBLUR
				|| image->width == 0
				|| image->height == 0
				|| image->width * image->height > NON_ZLIB_TEXTURE_MAX_PIXELS) {
			return 0;
		}

		image->pixels = malloc(image->width * image->height * sizeof(uint32_t));

		if (!image->pixels) {
			return 0;
		}

		switch (tex->images[i].compression) {
		case PDCOMPRESSION_UNCOMPRESSED0:
		case PDCOMPRESSION_UNCOMPRESSED1:
			texReadUncompressed(image);
			break;
		case PDCOMPRESSION_HUFFMAN:
			texInflateHuffman(scratch, g_TexFormatNumChannels[image->format] * image->width * image->height, g_TexFormatChannelSizes[image->format]);

			if (g_TexFormatHas1BitAlpha[image->format]) {
				texReadAlphaBits(&scratch[image->width * image->height * 3], image->width * image->height);
			}

			texChannelsToPixels(scratch, image->width, image->height, image->pixels, image->format);
			break;
		case PDCOMPRESSION_HUFFMANPERHCHANNEL:
			for (int j = 0; j < g_TexFormatNumChannels[image->format]; j++) {
				texInflateHuffman(&scratch[image->width * image->height * j], image->width * image->height, g_TexFormatChannelSizes[image->format]);
			}

			if (g_TexFormatHas1BitAlpha[image->format]) {
				texReadAlphaBits(&scratch[image->width * image->height * 3], image->width * image->height);
			}

			texChannelsToPixels(scratch, image->width, image->height, image->pixels, image->format);
			break;
		case PDCOMPRESSION_RLE:
			texInflateRle(scratch, g_TexFormatNumChannels[image->format] * image->width * image->height);

			if (g_TexFormatHas1BitAlpha[image->format]) {
				texReadAlphaBits(&scratch[image->width * image->height * 3], image->width * image->height);
			}

			texChannelsToPixels(scratch, image->width, image->height, image->pixels, image->format);
			break;
		case PDCOMPRESSION_LOOKUP:
			value = texBuildLookup(lookup, g_TexFormatBitsPerPixel[image->format]);

			if (value == 0) {
				return 0;
			}

			texInflateLookup(image->width, image->height, image->pixels, lookup, value, image->format);
			break;
		case PDCOMPRESSION_HUFFMANLOOKUP:
			value = texBuildLookup(lookup, g_TexFormatBitsPerPixel[image->format]);

			if (value == 0) {
				return 0;
			}

			texInflateHuffman(scratch, image->width * image->height, value);
			texInflateLookupFromBuffer(scratch, image->width, image->height, image->pixels, lookup, value, image->format);
			break;
		case PDCOMPRESSION_RLELOOKUP:
			value = texBuildLookup(lookup, g_TexFormatBitsPerPixel[image->format]);

			if (value == 0) {
				return 0;
			}

			texInflateRle(scratch, image->width * image->height);
			texInflateLookupFromBuffer(scratch, image->width, image->height, image->pixels, lookup, value, image->format);
			break;
		case PDCOMPRESSION_HUFFMANBLUR:
			value = texReadBits(3);
			texInflateHuffman(scratch, g_TexFormatNumChannels[image->format] * image->width * image->height, g_TexFormatChannelSizes[image->format]);
			texBlur(scratch, image->width, g_TexFormatNumChannels[image->format] * image->height, value, g_TexFormatChannelSizes[image->format]);

			if (g_TexFormatHas1BitAlpha[image->format]) {
				texReadAlphaBits(&scratch[image->width * image->height * 3], image->width * image->height);
			}

			texChannelsToPixels(scratch, image->width, image->height, image->pixels, image->format);
			break;
		case PDCOMPRESSION_RLEBLUR:
			value = texReadBits(3);
			texInflateRle(scratch, g_TexFormatNumChannels[image->format] * image->width * image->height);
			texBlur(scratch, image->width, g_TexFormatNumChannels[image->format] * image->height, value, g_TexFormatChannelSizes[image->format]);

			if (g_TexFormatHas1BitAlpha[image->format]) {
				texReadAlphaBits(&scratch[image->width * image->height * 3], image->width * image->height);
			}

			texChannelsToPixels(scratch, image->width, image->height, image->pixels, image->format);
			break;
		}

		if (var800ab548 == 0) {
			var800ab540++;
		} else {
			var800ab548 = 0;
		}
	}

	return 1;
}

static int texLoad(struct pd_tex *tex)
{
	int sp14a8 = texReadBits(1);
	int iszlib = texReadBits(1);
	int lod = texReadBits(6);

	if (lod > PDTEX_MAX_IMAGES) {
		lod = PDTEX_MAX_IMAGES;
	}

	tex->has_explicit_lods = sp14a8;
	tex->lod_count = lod;
	tex->num_images = sp14a8 && lod ? lod : 1;

	if (tex->num_images > PDTEX_MAX_IMAGES) {
		tex->num_images = PDTEX_MAX_IMAGES;
	}

	if (iszlib) {
		return texInflateZlib(tex, sp14a8, lod);
	} else {
		return texInflateNonZlib(tex, sp14a8, lod);
	}
}

int reader_read(FILE *fp, struct pd_tex *tex)
{
	long filelen;
	size_t len;
	uint8_t *buffer;

	if (fseek(fp, 0, SEEK_END) != 0) {
		return 0;
	}

	filelen = ftell(fp);

	if (filelen <= 0) {
		return 0;
	}

	len = filelen;

	if (fseek(fp, 0, SEEK_SET) != 0) {
		return 0;
	}

	buffer = malloc(len);

	if (!buffer || fread(buffer, len, 1, fp) != 1) {
		free(buffer);
		return 0;
	}

	texSetBitstring(buffer);

	if (!texLoad(tex)) {
		free(buffer);
		return 0;
	}

	free(buffer);

	return 1;
}
