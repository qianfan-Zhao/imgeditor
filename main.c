/*
 * imgeditor tools
 *
 * qianfan Zhao 2022-09-22
 */
#define _GNU_SOURCE /* for memmem */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "imgeditor.h"
#include "string_helper.h"
#include "structure.h"
#include "minilzo.h"

const char *smart_format_size(uint64_t byte_addr, char *buf, size_t bufsz)
{
	uint64_t addr;
	const char *units;

	if (byte_addr <= SIZE_KB(1) || (byte_addr & SIZEMASK_KB)) {
		addr = byte_addr;
		units = "B  ";
	} else if (byte_addr & SIZEMASK_MB) {
		addr = byte_addr / SIZE_KB(1);
		units = "KiB";
	} else if (byte_addr & SIZEMASK_GB) {
		addr = byte_addr / SIZE_MB(1);
		units = "MiB";
	} else {
		addr = byte_addr / SIZE_GB(1);
		units = "GiB";
	}

	snprintf(buf, bufsz, "%" PRIu64 "%s", addr, units);
	return buf;
}

int fileopen(const char *file, int flags, mode_t mode)
{
	int fd = open(file, flags, mode);

	if (fd < 0)
		fprintf(stderr, "Error: open %s failed(%m)\n", file);

	return fd;
}

int64_t filelength(int fd)
{
	struct virtual_file *vf = virtual_file_get(fd);

	if (vf)
		return vf->total_length;

	/* lseek can't work on 32bit ARM platform if the file is larger than
	 * 2GB, it will report EOVERFLOW.
	 * let's use lseek64 instead of lseek.
	 */
	int64_t sz = lseek64(fd, 0, SEEK_END);

	lseek64(fd, 0, SEEK_SET);
	return sz;
}

off64_t filestart(int fd)
{
	struct virtual_file *vf = virtual_file_get(fd);

	if (!vf)
		return 0;

	return vf->start_offset;
}

off64_t fileseek(int fd, off64_t offset)
{
	return lseek64(fd, filestart(fd) + offset, SEEK_SET);
}

/* Return zero on successful */
int fileread(int fd, void *buf, size_t sz)
{
	size_t n = 0;

	while (n < sz) {
		int ret = read(fd, buf + n, sz - n);

		if (ret < 0)
			return ret;

		if (ret == 0) /* end of file */
			break;

		n += ret;
	}

	return n == sz ? 0 : -1;
}

static LIST_HEAD(registed_imgeditor_lists);

static int editor_prepare_private_data(struct imgeditor *editor)
{
	if (!editor->private_data && editor->private_data_size > 0) {
		/* alloc private data first */
		editor->private_data = malloc(editor->private_data_size);
		if (!editor->private_data) {
			fprintf(stderr, "Error: alloc %zu bytes private data for %s failed\n",
				editor->private_data_size, editor->name);
			return -1;
		}
	}

	return 0;
}

static int editor_init(struct imgeditor *editor)
{
	int ret = 0;

	if (editor->init) {
		ret = editor->init(editor->private_data);
		if (ret < 0)
			fprintf(stderr, "Error: init %s failed\n", editor->name);
	}

	return ret;
}

void register_imgeditor(struct imgeditor *editor)
{
	list_init(&editor->head);

	if (!editor_prepare_private_data(editor)) {
		if (!editor_init(editor))
			list_add_tail(&editor->head, &registed_imgeditor_lists);
		else
			free(editor->private_data);
	}
}

static void editor_exit(struct imgeditor *editor)
{
	if (editor->private_data) {
		if (editor->exit)
			editor->exit(editor->private_data);
		free(editor->private_data);
		editor->private_data = NULL;
	}
}

static struct imgeditor *get_imgeditor_byname(const char *name)
{
	struct imgeditor *editor;

	list_for_each_entry(editor, &registed_imgeditor_lists, head,
			    struct imgeditor) {
		if (!strcmp(editor->name, name))
			return editor;
	}

	return NULL;
}

struct img_location {
	const char		*name;
	int64_t			offset;
};

static int img_location_compare(const void *p1, const void *p2)
{
	const struct img_location *img1 = p1, *img2 = p2;

	return img1->offset - img2->offset;
}

static void print_img_location(struct img_location *img)
{
	char s_offset[128], s_sector[128];

	snprintf(s_offset, sizeof(s_offset),
		 "0x%08" PRIx64 "(%" PRIu64 ")",
		 img->offset, img->offset);

	snprintf(s_sector, sizeof(s_sector),
		"0x%08" PRIx64 "(%" PRIu64 ")",
		 img->offset / 512,
		 img->offset / 512);

	printf("%-20s %-25s %-25s\n", img->name, s_offset, s_sector);
}

static struct img_location *imgeditor_search_buf(int fd, off64_t file_offset,
						 const void *buf, size_t bufsz,
						 int *count)
{
	struct img_location *imgs = NULL;
	int found = 0;
	int ready;

	do {
		struct imgeditor *editor;

		ready = 0;

		list_for_each_entry(editor, &registed_imgeditor_lists, head,
				    struct imgeditor) {
			struct imgmagic *sm = &editor->search_magic;
			int64_t img_offset, magic_file_offset;
			int buf4m_search_offset;
			const void *p_magic;
			int detect;
			int vfd;

			if (sm->magic_sz == 0)
				continue;

			if (sm->next_search_offset == 0) {
				buf4m_search_offset = 0;
			} else {
				buf4m_search_offset =
					sm->next_search_offset - file_offset;
			}

			if (buf4m_search_offset < 0 ||
				buf4m_search_offset >= (int)bufsz)
				continue;

			p_magic = memmem(buf + buf4m_search_offset,
					 bufsz - buf4m_search_offset,
					 sm->magic, sm->magic_sz);
			if (!p_magic) {
				sm->next_search_offset = file_offset + bufsz;
				continue;
			}

			/* the magic is matched */
			++ready;
			magic_file_offset = file_offset + (p_magic - buf);
			sm->next_search_offset = magic_file_offset + 1;
			img_offset = magic_file_offset - sm->magic_offset;

			if (get_verbose_level() > 2) {
				/* print the raw address of this magic */
				printf("? %-18s 0x%" PRIx64 "\n",
					editor->name,
					img_offset + sm->magic_offset);
			}

			if (img_offset + (int)editor->header_size >= filelength(fd))
				continue;

			structure_force_endian(STRUCTURE_ENDIAN_FORCE_NONE);

			memset(editor->private_data, 0,
				editor->private_data_size);

			if (editor->init)
				editor->init(editor->private_data);

			vfd = virtual_file_dup(fd, img_offset);
			if (vfd < 0)
				continue;

			detect = editor->detect(editor->private_data, 0, vfd);
			if (detect == 0) {
				imgs = realloc(imgs,
					       sizeof(*imgs) * (found + 1));

				if (!imgs) {
					fprintf(stderr, "Error: alloc %d imgs"
						" failed\n", found);
					found = 0;
					break;
				}

				imgs[found].name = editor->name;
				imgs[found].offset = img_offset;
				found++;

				/* some driver such as sunxi_package will
				 * alloc data when @detect,
				 * we should free those
				 */
				if (editor->exit)
					editor->exit(editor->private_data);
			}

			virtual_file_close(vfd);
		}
	} while (ready);

	*count = found;
	return imgs;
}

static int imgeditor_search_foreach(int fd)
{
	#define BUF4M_SZ (4 << 20)
	uint8_t *buf4m = malloc(BUF4M_SZ);
	off64_t offset = filestart(fd);
	int loaded, count = 0;

	if (!buf4m) {
		fprintf(stderr, "Error: alloc buf4m failed\n");
		return -1;
	}

	do {
		struct img_location *imgs;
		int found = 0;

		/* read back: 1024 is OK for all magics */
		if (offset > 1024)
			offset -= 1024;

		lseek64(fd, offset, SEEK_SET);
		loaded = read(fd, buf4m, BUF4M_SZ);
		if (loaded < 0) {
			fprintf(stderr, "Error: read from offset #%" PRIu64
				"failed\n", offset);
			break;
		} else if (loaded == 0) {
			break;
		}

		imgs = imgeditor_search_buf(fd, offset, buf4m, loaded, &found);
		offset += loaded;

		if (!found)
			continue;

		if (count == 0 && found) {
			/* print the title */
			printf("%-20s %-25s %-25s\n",
				"NAME", "OFFSET", "SECTOR");
		}

		qsort(imgs, found, sizeof(*imgs), img_location_compare);
		for (int i = 0; i < found; i++)
			print_img_location(&imgs[i]);

		free(imgs);
		count += found;
	} while (loaded == BUF4M_SZ);

	free(buf4m);
	return count;
}

static int imgeditor_search(const char *name, off64_t offset)
{
	int fd = virtual_file_open(name, O_RDONLY, 0, offset);
	int search_count;

	if (fd < 0)
		return fd;

	if (filelength(fd) <= 0) {
		virtual_file_close(fd);
		return -1;
	}

	search_count = imgeditor_search_foreach(fd);
	if (search_count <= 0) /* noting is found */
		return -1;

	return 0;
}

static void usage(void)
{
	const struct imgeditor *editor;

	fprintf(stderr, "imgeditor: firmware image edit tools\n");
	fprintf(stderr, "Usage: imgeditor [OPTIONS] [outfile] [-- SUBOPTIONS]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   --offset offset     set the offset location\n");
	fprintf(stderr, "   --sector sector     set the offset location in sectors\n");
	fprintf(stderr, "   --sector-size sz    set the sector's size, default is 512\n");
	fprintf(stderr, "   --unpack image      unpack all\n");
	fprintf(stderr, "   --pack firmware-dir pack firmwares to a image file\n");
	fprintf(stderr, "   --type type         select the image type\n");
	fprintf(stderr, "-s --search            search supported images\n");
	fprintf(stderr, "-v --verbose:          set the verbose mode\n");
	fprintf(stderr, "-h --help              show this messages\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "Available image types:\n");
	list_for_each_entry(editor, &registed_imgeditor_lists, head,
			    const struct imgeditor) {
		fprintf(stderr, "  %-20s %s\n",
			editor->name, editor->descriptor);
	}
}

static int option_verbose = 0;

int get_verbose_level(void)
{
	return option_verbose;
}

enum {
	ARG_TYPE,
	ARG_VERBOSE,
	ARG_OFFSET,
	ARG_SECTOR,
	ARG_SECTOR_SIZE,

	ACTION_UNPACK,
	ACTION_PACK,

	ACTION_SEARCH = 's',

	ACTION_LIST = 'l',
	ACTION_HELP = 'h',
};

static struct option imgeditor_options[] = {
	/* name			has_arg,		*flag,	val */
	{ "type",		required_argument,	NULL,	ARG_TYPE	},
	{ "offset",		required_argument,	NULL,	ARG_OFFSET	},
	{ "sector",		required_argument,	NULL,	ARG_SECTOR	},
	{ "sector-size",	required_argument,	NULL,	ARG_SECTOR_SIZE	},
	{ "unpack",		required_argument,	NULL,	ACTION_UNPACK	},
	{ "pack",		required_argument,	NULL,	ACTION_PACK	},
	{ "search",		no_argument,		NULL,	ACTION_SEARCH	},
	{ "verbose",		no_argument,		NULL,	ARG_VERBOSE	},
	{ "help",		no_argument,		NULL,	ACTION_HELP	},
	{ NULL,			0,			NULL,	0		},
};

static int editor_detect(const struct imgeditor *editor, int force_type, int fd)
{
	int64_t filesize = filelength(fd);

	if (filesize < (long)editor->header_size)
		return -1;

	/* this image editor can't auto detect, it only be used with '--type' */
	if (!editor->detect)
		return -1;

	return editor->detect(editor->private_data, force_type, fd);
}

static int arg_to_ull(const char *arg, const char *value,
		      unsigned long long *ull)
{
	unsigned long long n;
	int err;

	n = strict_strtoull(value, 0, &err, NULL);
	if (err) {
		fprintf(stderr, "Error: bad %s %s\n", arg, value);
		return -1;
	}

	*ull = n;
	return 0;
}

static int arg_to_ul(const char *arg, const char *value, unsigned long *ul)
{
	unsigned long long n;
	int ret;

	ret = arg_to_ull(arg, value, &n);
	if (ret < 0)
		return ret;

	*ul = (unsigned long)n;
	return 0;
}

int main(int argc, char *argv[])
{
	struct imgeditor *editor = NULL;
	const char *origin_file = NULL, *out_file = NULL, *type = NULL;
	char tmpbuf[1024];
	unsigned long long offset = 0;
	unsigned long offset_sector = 0, sector_size = 512;
	int main_argc = 0, sub_argc = argc;
	int search_mode = 0, action = ACTION_LIST; /* default action */
	int fd = -1;
	int ret = -1;

	lzo_init();

	/* args after '--' will passed to the subcommand */
	for (main_argc = 0; main_argc < argc; main_argc++, sub_argc--) {
		if (!strcmp(argv[main_argc], "--")) {
			sub_argc--; /* skip '--' */
			argv[main_argc] = NULL;
			break;
		}
	}

	while (1) {
		int option_index = 0;
		int c;

		c = getopt_long(main_argc, argv, "hvs", imgeditor_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case ARG_TYPE:
			type = optarg;
			break;
		case ARG_OFFSET:
			ret = arg_to_ull("--offset", optarg, &offset);
			if (ret < 0)
				return ret;
			break;
		case ARG_SECTOR:
			ret = arg_to_ul("--sector", optarg, &offset_sector);
			if (ret < 0)
				return ret;
			break;
		case ARG_SECTOR_SIZE:
			ret = arg_to_ul("--sector-size", optarg, &sector_size);
			if (ret < 0)
				return ret;
			break;
		case 'v':
		case ARG_VERBOSE:
			option_verbose++;
			break;
		case ACTION_LIST:
		case ACTION_PACK:
		case ACTION_UNPACK:
			origin_file = optarg;
			action = c;
			break;
		case ACTION_SEARCH:
			search_mode = 1;
			break;
		case ACTION_HELP:
			usage();
			break;
		default:
			usage();
			return -1;
		}
	}

	if (main_argc == 1) {
		usage();
		return -1;
	}

	if (optind < argc)
		out_file = argv[optind];

	if (offset_sector)
		offset = offset_sector * sector_size;

	if (search_mode)
		return imgeditor_search(out_file, offset);

	switch (action) {
	case ACTION_LIST:
		origin_file = out_file; /* imgeditor xxx.img */
		/* fallthrough */
	case ACTION_UNPACK:
		fd = virtual_file_open(origin_file, O_RDONLY, 0, offset);
		if (fd < 0)
			return fd;

		if (type) {
			editor = get_imgeditor_byname(type);
			if (!editor) {
				fprintf(stderr, "Error: image type %s is unsupported\n",
					type);
				return -1;
			}

			if (editor_detect(editor, 1, fd) < 0)
				goto done;
		} else {
			int matched = 0;

			list_for_each_entry(editor, &registed_imgeditor_lists,
					    head, struct imgeditor) {
				lseek64(fd, filestart(fd), SEEK_SET);

				if (!editor_detect(editor, 0, fd)) {
					matched = 1;
					break;
				}
			}

			if (!matched) {
				fprintf(stderr, "Error: can't detect the file type of %s\n",
					origin_file);
				goto done;
			}
		}

		/* recovery fd after detect */
		lseek64(fd, filestart(fd), SEEK_SET);
		break;
	case ACTION_PACK:
		if (!out_file) {
			fprintf(stderr, "Error: the output file is not selected\n");
			goto done;
		}

		if (!type) {
			/* reading the .imgeditor marker from firmware_dir */
			snprintf(tmpbuf, sizeof(tmpbuf), "%s/.imgeditor",
				 origin_file);
			fd = open(tmpbuf, O_RDONLY);
			if (fd < 0)
				return fd;

			memset(tmpbuf, 0, sizeof(tmpbuf));
			read(fd, tmpbuf, sizeof(tmpbuf));
			type = tmpbuf;
			close(fd);
		}

		editor = get_imgeditor_byname(type);
		if (!editor) {
			fprintf(stderr, "Error: image type %s is unsupported\n",
				type);
			return -1;
		}

		/* fd point to the output image now */
		fd = virtual_file_open(out_file, O_RDWR | O_CREAT, 0664, 0);
		if (fd < 0)
			return fd;

		/* clear it */
		ftruncate(fd, 0);
		break;
	}

	/* A program that scans multiple argument vectors, or rescans the same vector
	 * more than once, must reinitialize getopt() by resetting optind to 0,
	 * rather than the traditional value of 1.
	 */
	optind = 0;

	switch (action) {
	case ACTION_LIST:
		if ((editor->flags & IMGEDITOR_FLAG_HIDE_INFO_WHEN_LIST) == 0)
			printf("%s: %s\n", editor->name, editor->descriptor);
		if (editor->list)
			ret = editor->list(editor->private_data, fd,
					   sub_argc, &argv[main_argc + 1]);
		break;
	case ACTION_UNPACK:
		umask(0);

		if (editor->flags & IMGEDITOR_FLAG_CONTAIN_MULTI_BIN) {
			/* create a dump directory for saving multi binary */
			snprintf(tmpbuf, sizeof(tmpbuf), "%s.dump", origin_file);
			mkdir(tmpbuf, 0744);
		} else {
			if (!out_file) {
				fprintf(stderr, "Error: outfile is requested for image type %s\n",
					editor->name);
				goto done;
			}
			snprintf(tmpbuf, sizeof(tmpbuf), "%s", out_file);
		}

		if (editor->unpack) {
			ret = editor->unpack(editor->private_data, fd, tmpbuf,
					     sub_argc, &argv[main_argc + 1]);
			if (!ret && (editor->flags & IMGEDITOR_FLAG_CONTAIN_MULTI_BIN)) {
				/* create a type marker file */
				FILE *fp;

				strncat(tmpbuf, "/.imgeditor", sizeof(tmpbuf) - 1);
				fp = fopen(tmpbuf, "w+");
				if (!fp) {
					fprintf(stderr, "Error: write %s failed\n",
						tmpbuf);
					goto done;
				}

				fprintf(fp, "%s", editor->name);
				fclose(fp);
			}
		}
		break;
	case ACTION_PACK:
		if (editor->pack)
			ret = editor->pack(editor->private_data, origin_file, fd,
					   sub_argc, &argv[main_argc + 1]);
		break;
	}

done:
	if (!(fd < 0))
		virtual_file_close(fd);

	list_for_each_entry(editor, &registed_imgeditor_lists, head, struct imgeditor)
		editor_exit(editor);

	return ret;
}
