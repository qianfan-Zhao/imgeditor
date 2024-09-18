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
