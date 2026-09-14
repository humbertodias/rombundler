/* Stdio stand-ins for libretro-common streams/path helpers that static
 * cores (e.g. Genesis Plus GX + libchdr) expect the frontend to provide. */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "libretro.h"
#include "platform.h"

typedef FILE RFILE;

RFILE *rfopen(const char *path, const char *mode)
{
	return platform_fopen(path, mode);
}

int rfclose(RFILE *stream)
{
	if (!stream)
		return EOF;
	return fclose(stream);
}

int64_t rftell(RFILE *stream)
{
	if (!stream)
		return -1;
	return (int64_t)ftell(stream);
}

int64_t rfseek(RFILE *stream, int64_t offset, int origin)
{
	if (!stream)
		return -1;
	return fseek(stream, (long)offset, origin);
}

int64_t rfread(void *buffer, size_t elem_size, size_t elem_count, RFILE *stream)
{
	if (!stream || !elem_size || !elem_count)
		return 0;
	return (int64_t)fread(buffer, elem_size, elem_count, stream);
}

char *rfgets(char *s, int maxCount, RFILE *stream)
{
	if (!stream)
		return NULL;
	return fgets(s, maxCount, stream);
}

void filestream_vfs_init(const struct retro_vfs_interface_info *vfs_info)
{
	(void)vfs_info;
}

RFILE *filestream_open(const char *path, unsigned mode, unsigned hints)
{
	const char *m = "rb";

	(void)hints;
	if ((mode & RETRO_VFS_FILE_ACCESS_READ) && (mode & RETRO_VFS_FILE_ACCESS_WRITE))
		m = (mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING) ? "r+b" : "w+b";
	else if (mode & RETRO_VFS_FILE_ACCESS_WRITE)
		m = (mode & RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING) ? "ab" : "wb";
	else
		m = "rb";
	return platform_fopen(path, m);
}

int filestream_close(RFILE *stream)
{
	return rfclose(stream);
}

int64_t filestream_tell(RFILE *stream)
{
	return rftell(stream);
}

int64_t filestream_seek(RFILE *stream, int64_t offset, int seek_position)
{
	int origin = SEEK_SET;

	switch (seek_position) {
	case RETRO_VFS_SEEK_POSITION_CURRENT:
		origin = SEEK_CUR;
		break;
	case RETRO_VFS_SEEK_POSITION_END:
		origin = SEEK_END;
		break;
	default:
		origin = SEEK_SET;
		break;
	}
	return rfseek(stream, offset, origin);
}

int64_t filestream_read(RFILE *stream, void *data, int64_t len)
{
	if (!stream || len <= 0)
		return 0;
	return (int64_t)fread(data, 1, (size_t)len, stream);
}

int64_t filestream_write(RFILE *stream, const void *data, int64_t len)
{
	if (!stream || len <= 0)
		return 0;
	return (int64_t)fwrite(data, 1, (size_t)len, stream);
}

char *filestream_gets(RFILE *stream, char *s, size_t len)
{
	if (!stream || !s || !len)
		return NULL;
	return fgets(s, (int)len, stream);
}

size_t strlcpy_retro__(char *dest, const char *source, size_t size)
{
	size_t src_len;

	if (!dest || !size)
		return source ? strlen(source) : 0;
	src_len = source ? strlen(source) : 0;
	if (src_len + 1 < size) {
		if (source)
			memcpy(dest, source, src_len + 1);
		else
			dest[0] = '\0';
	} else {
		if (source && size > 1)
			memcpy(dest, source, size - 1);
		dest[size - 1] = '\0';
	}
	return src_len;
}

size_t strlcat_retro__(char *dest, const char *source, size_t size)
{
	size_t dest_len;
	size_t src_len;

	if (!dest || !size)
		return source ? strlen(source) : 0;
	dest_len = strlen(dest);
	src_len = source ? strlen(source) : 0;
	if (dest_len >= size)
		return size + src_len;
	return dest_len + strlcpy_retro__(dest + dest_len, source, size - dest_len);
}

size_t fill_pathname_join(char *out_path, const char *dir, const char *path, size_t size)
{
	size_t n;

	if (!out_path || !size)
		return 0;
	out_path[0] = '\0';
	if (dir && dir[0])
		strlcpy_retro__(out_path, dir, size);
	n = strlen(out_path);
	if (n && out_path[n - 1] != '/' && out_path[n - 1] != '\\' && n + 1 < size) {
		out_path[n] = '/';
		out_path[n + 1] = '\0';
	}
	if (path && path[0])
		strlcat_retro__(out_path, path, size);
	return strlen(out_path);
}
