/*
 * u-boot env image editor
 * qianfan Zhao 2023-04-24
 */

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "imgeditor.h"
#include "string_helper.h"

#define ENV_PART_MAIN		0
#define ENV_PART_REDUND		1

struct uenv_editor_private_data {
	long env_size;
};

#define UENV_MINIMUM_SIZE	2048
#define UENV_MAXIMUM_SIZE	SIZE_MB(2)

void uenv_calc_crc(uint8_t *buf, size_t sz, void *p)
{
	uint32_t *p_crc = p;

	*p_crc = crc32(*p_crc, buf, sz);
}

static int uenv_auto_detect_filesize(int fd, long *filesz)
{
	const int better_sz = SIZE_KB(128);
	uint32_t crc, crc_expected;
	int64_t sz = filelength(fd);
	uint8_t buf[sizeof(crc)];

	if (sz >= UENV_MAXIMUM_SIZE) {
		sz = UENV_MAXIMUM_SIZE;
	} else {
		sz = aligned_length(sz, better_sz);
	}

	/* crc saved in little endian */
	read(fd, buf, sizeof(buf));
	crc_expected = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);

	while (sz > UENV_MINIMUM_SIZE) {
		crc = 0;

		dd(fd, -1, sizeof(crc), -1, sz - sizeof(crc),
			uenv_calc_crc, &crc);
		if (crc == crc_expected) {
			*filesz = sz;
			return 0;
		}

		if (sz > better_sz)
			sz -= better_sz;
		else
			sz = sz / 2;
	}

	return -1;
}

static int uenv_detect(void *private_data, int force_type, int fd)
{
	struct uenv_editor_private_data *p = private_data;

	if (!force_type)
		return -1;

	return uenv_auto_detect_filesize(fd, &p->env_size);
}

static int uenv_list(void *private_data, int fd, int argc, char **argv)
{
	struct uenv_editor_private_data *p = private_data;
	char *s, *buf = malloc(p->env_size);

	if (!buf) {
		fprintf(stderr, "Error: malloc failed\n");
		return -1;
	}

	/* loading the whole env and skip crc32 header */
	read(fd, buf, p->env_size);
	s = buf + sizeof(uint32_t);

	printf("#uEnv %ld ", p->env_size);
	switch (*s) {
	case ENV_PART_MAIN:
	case ENV_PART_REDUND:
		printf("%d", *s);
		s++;
		break;
	}
	putchar('\n');

	while (s - buf < p->env_size - 1) {
		switch (*s) {
		case '\r':
			printf("\\r");
			break;
		case '\n':
			printf("\\n\n");
			break;
		case '\0':
			putchar('\n');
			if (s[1] == '\0')
				goto done;
			break;
		default:
			putchar(*s);
			break;
		}
		s++;
	}

done:
	free(buf);

	return 0;
}

static int uenv_pack(void *p, const char *uenv_filename, int fd_outimg,
		     int argc, char **argv)
{
	FILE *fp = fopen(uenv_filename, "r");
	int read_title = 0, env_part = -1;
	long env_size = 0;
	char buffer[1024];
	uint32_t crc = 0;

	if (!fp) {
		fprintf(stderr, "Error: read %s failed(%m)\n",
			uenv_filename);
		return -1;
	}

	while (fgets(buffer, sizeof(buffer) - 1, fp)) {
		int len = strlen(buffer);

		if (buffer[len - 1] == '\n') {
			buffer[len - 1] = '\0';
			len--;
		}

		if (!read_title) {
			/* #uEnv env_size env_part */
			int fd_zero, n;
			uint8_t part;

			n = sscanf(buffer, "#uEnv %ld %d", &env_size, &env_part);
			if (n < 1) {
				fprintf(stderr, "Error: Bad uEnv title (%s)\n",
					buffer);
				return -1;
			}

			if (env_size % 2) {
				fprintf(stderr, "Error: env size(%ld) is not power2\n",
					env_size);
				return -1;
			} else if (n == 2) {
				switch (env_part) {
				case ENV_PART_MAIN:
				case ENV_PART_REDUND:
					break;
				default:
					fprintf(stderr, "Error: bad part %d\n",
						env_part);
					return -1;
				}
			}

			/* fill fd_outimg with zero */
			fd_zero = open("/dev/zero", O_RDONLY);
			dd(fd_zero, fd_outimg, 0, 0, env_size, NULL, NULL);
			close(fd_zero);

			lseek(fd_outimg, sizeof(crc), SEEK_SET);
			switch (env_part) {
			case ENV_PART_MAIN:
			case ENV_PART_REDUND:
				part = env_part;
				write(fd_outimg, &part, sizeof(part));
				break;
			}

			read_title = 1;
			continue;
		}

		string_replace_in_buffer(buffer, sizeof(buffer), "\\r", "\r");
		string_replace_in_buffer(buffer, sizeof(buffer), "\\n", "\n");

		/* write buffer string including '\0' */
		write(fd_outimg, buffer, strlen(buffer) + 1);
	}

	if (filelength(fd_outimg) > env_size) {
		fprintf(stderr, "Error: env size overflow\n");
		return -1;
	}

	dd(fd_outimg, -1, sizeof(crc), 0, env_size - sizeof(crc),
		uenv_calc_crc, &crc);
	crc = cpu_to_le32(crc);
	lseek(fd_outimg, 0, SEEK_SET);
	write(fd_outimg, &crc, sizeof(crc));

	return 0;
}

static struct imgeditor uenv_editor = {
	.name			= "uenv",
	.descriptor		= "u-boot env image editor",
	.header_size		= UENV_MINIMUM_SIZE,
	.flags			= IMGEDITOR_FLAG_HIDE_INFO_WHEN_LIST,
	.private_data_size	= sizeof(struct uenv_editor_private_data),
	.detect			= uenv_detect,
	.list			= uenv_list,
	.pack			= uenv_pack,
};
REGISTER_IMGEDITOR(uenv_editor);
