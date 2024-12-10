/*
 * print buffer as 'hexdump -C'
 * qianfan Zhao
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define HEXDUMP_PER_LINE 16
#define aligned_length(len, align)	(((len) + (align) - 1) / (align) * (align))

void hexdump_indent(const char *indent_fmt, const void *buf, size_t sz,
		    unsigned long baseaddr)
{
	uint8_t preline[HEXDUMP_PER_LINE];
	const uint8_t *blk = buf;
	int skip = 0;

	for (size_t i = 0; i < sz; i += HEXDUMP_PER_LINE) {
		int sz_buster = sz - i;

		if (sz_buster > HEXDUMP_PER_LINE)
			sz_buster = HEXDUMP_PER_LINE;

		if (i != 0 && !memcmp(preline, &blk[i], sz_buster)) {
			if (!skip) {
				skip = 1;
				if (indent_fmt)
					printf(indent_fmt, "");
				printf("*\n");
			}
			continue;
		} else {
			if (i != 0 && indent_fmt)
				printf(indent_fmt, "");
			skip = 0;
		}

		/* the block's address */
		printf("%08lx ", baseaddr + i);

		for (size_t n = 0; n < 2; n++) {
			for (size_t j = 0; j < HEXDUMP_PER_LINE; j++) {
				int overflow = 0;

				if (i + j >= sz)
					overflow = 1;

				switch (n) {
				case 0:
					if (!overflow)
						printf("%02x ", blk[i + j]);
					else
						printf("   ");
					break;
				case 1:
					if (j == 0)
						printf("|");
					if (!overflow) {
						printf("%c",
							isprint(blk[i + j]) ?
							blk[i + j] : '.');
					} else {
						printf(" ");
					}
					if (j == HEXDUMP_PER_LINE - 1)
						printf("|");
					break;
				}
			}
		}

		memcpy(preline, &blk[i], sz_buster);
		printf("\n");
	}

	if (indent_fmt)
		printf(indent_fmt, "");
	printf("%08lx\n", baseaddr + aligned_length(sz, HEXDUMP_PER_LINE));
}

void hexdump(const void *buf, size_t sz, unsigned long baseaddr)
{
	hexdump_indent(NULL, buf, sz, baseaddr);
}
