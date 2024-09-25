/*
 * misc functions
 */
#include <string.h>
#include "imgeditor.h"
#include "string_helper.h"
#include "gd_private.h"

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

static struct imgeditor *imgeditor_editor_request(const char *name)
{
	struct imgeditor *editor = gd_get_imgeditor(name);

	if (!editor)
		return editor;

	memset(editor->private_data, 0, editor->private_data_size);
	if (editor->init)
		editor->init(editor->private_data);

	return editor;
}

static void imgeditor_editor_release(struct imgeditor *editor)
{
	if (editor->exit)
		editor->exit(editor->private_data);

	/* reset the private data to pervent editor_exit@main
	 * double free again
	 */
	memset(editor->private_data, 0, editor->private_data_size);
	if (editor->init)
		editor->init(editor->private_data);
}

int imgeditor_editor_detect(const char *name, int fd)
{
	struct imgeditor *editor = gd_get_imgeditor(name);
	int ret;

	if (!editor)
		return -1;

	ret = editor->detect(editor->private_data, 0 /* force_type */, fd);
	imgeditor_editor_release(editor);

	return ret;
}

int imgeditor_editor_main(const char *name, const char *args)
{
	struct imgeditor *editor = imgeditor_editor_request(name);
	char args_nonconst[8192], *argv[128] = { NULL };
	int argc, ret = -1;

	if (!editor || !editor->main)
		return ret;

	snprintf(args_nonconst, sizeof(args_nonconst), "%s", args);
	argc = argv_buffer_split_with_delim(args_nonconst, " \t", argv, 128);
	ret = editor->main(editor->private_data, argc, argv);

	imgeditor_editor_release(editor);
	return ret;
}

#define editor_argv_helper(func, fd, ...)					\
	struct imgeditor *editor = imgeditor_editor_request(name);		\
	char args_nonconst[8192], *argv[128] = { NULL };			\
	off64_t cur_offset;							\
	int argc, ret = -1;							\
										\
	if (!editor || !editor->func)						\
		return ret;							\
										\
	cur_offset = lseek64(fd, 0, SEEK_CUR);					\
	ret = editor->detect(editor->private_data, 1, fd);			\
	if (ret < 0)								\
		goto done;							\
										\
	snprintf(args_nonconst, sizeof(args_nonconst), "%s", args);		\
	argc = argv_buffer_split_with_delim(args_nonconst, " \t", argv, 128);	\
										\
	/* restore file offset */						\
	lseek64(fd, cur_offset, SEEK_SET);					\
	ret = editor->func(editor->private_data, fd, ##__VA_ARGS__, argc, argv);\
										\
done:										\
	imgeditor_editor_release(editor);					\
	return ret;

int imgeditor_editor_unpack(const char *name, int fd, const char *out,
			    const char *args)
{
	editor_argv_helper(unpack, fd, out);
}

int imgeditor_editor_unpack2fd(const char *name, int fd, int fd_out,
			       const char *args)
{
	editor_argv_helper(unpack2fd, fd, fd_out);
}
