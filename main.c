/*
 * imgeditor tools
 *
 * qianfan Zhao 2022-09-22
 */
#define _LARGEFILE64_SOURCE
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

int fileopen(const char *file, int flags, mode_t mode)
{
	int fd = open(file, flags, mode);

	if (fd < 0)
		fprintf(stderr, "Error: open %s failed(%m)\n", file);

	return fd;
}

int64_t filelength(int fd)
{
	/* lseek can't work on 32bit ARM platform if the file is larger than
	 * 2GB, it will report EOVERFLOW.
	 * let's use lseek64 instead of lseek.
	 */
	int64_t sz = lseek64(fd, 0, SEEK_END);

	lseek64(fd, 0, SEEK_SET);
	return sz;
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

static void usage(void)
{
	const struct imgeditor *editor;

	fprintf(stderr, "imgeditor: firmware image edit tools\n");
	fprintf(stderr, "Usage: imgeditor [OPTIONS] [outfile] [-- SUBOPTIONS]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "   --unpack image      unpack all\n");
	fprintf(stderr, "   --pack firmware-dir pack firmwares to a image file\n");
	fprintf(stderr, "   --type type         select the image type\n");
	fprintf(stderr, "-h --help              show this messages\n");
	fprintf(stderr, "\n");

	fprintf(stderr, "Available image types:\n");
	list_for_each_entry(editor, &registed_imgeditor_lists, head,
			    const struct imgeditor) {
		fprintf(stderr, "  %-10s %s\n",
			editor->name, editor->descriptor);
	}
}

enum {
	ARG_TYPE,

	ACTION_UNPACK,
	ACTION_PACK,

	ACTION_LIST = 'l',
	ACTION_HELP = 'h',
};

static struct option imgeditor_options[] = {
	/* name			has_arg,		*flag,	val */
	{ "type",		required_argument,	NULL,	ARG_TYPE	},
	{ "unpack",		required_argument,	NULL,	ACTION_UNPACK	},
	{ "pack",		required_argument,	NULL,	ACTION_PACK	},
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

int main(int argc, char *argv[])
{
	struct imgeditor *editor = NULL;
	const char *origin_file = NULL, *out_file = NULL, *type = NULL;
	char tmpbuf[1024];
	int main_argc = 0, sub_argc = argc;
	int action = ACTION_LIST; /* default action */
	int fd = -1;
	int ret = -1;

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

		c = getopt_long(main_argc, argv, "h", imgeditor_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case ARG_TYPE:
			type = optarg;
			break;
		case ACTION_LIST:
		case ACTION_PACK:
		case ACTION_UNPACK:
			origin_file = optarg;
			action = c;
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

	switch (action) {
	case ACTION_LIST:
		origin_file = out_file; /* imgeditor xxx.img */
		/* fallthrough */
	case ACTION_UNPACK:
		fd = fileopen(origin_file, O_RDONLY, 0);
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
		lseek(fd, 0, SEEK_SET);
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
		fd = fileopen(out_file, O_RDWR | O_CREAT, 0664);
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
		close(fd);

	list_for_each_entry(editor, &registed_imgeditor_lists, head, struct imgeditor)
		editor_exit(editor);

	return ret;
}
