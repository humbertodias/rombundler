/* Stdio stand-ins for libretro-common streams/path helpers that static
 * cores (e.g. Genesis Plus GX + libchdr) expect the frontend to provide.
 * Nothing in the frontend calls these, so the WASM loader must keep them
 * alive or a side module's first call hits an empty Emscripten stub. */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libretro.h"
#include "platform.h"

#if defined(__EMSCRIPTEN__) && defined(ROMBUNDLER_WASM_DYNAMIC)
#include <zlib.h>
#endif

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#define RB_KEEP EMSCRIPTEN_KEEPALIVE
#else
#define RB_KEEP
#endif

typedef FILE RFILE;

RB_KEEP RFILE *rfopen(const char *path, const char *mode)
{
	return platform_fopen(path, mode);
}

RB_KEEP int rfclose(RFILE *stream)
{
	if (!stream)
		return EOF;
	return fclose(stream);
}

RB_KEEP int64_t rftell(RFILE *stream)
{
	if (!stream)
		return -1;
	return (int64_t)ftell(stream);
}

RB_KEEP int64_t rfseek(RFILE *stream, int64_t offset, int origin)
{
	if (!stream)
		return -1;
	return fseek(stream, (long)offset, origin);
}

RB_KEEP int64_t rfread(void *buffer, size_t elem_size, size_t elem_count, RFILE *stream)
{
	if (!stream || !elem_size || !elem_count)
		return 0;
	return (int64_t)fread(buffer, elem_size, elem_count, stream);
}

RB_KEEP char *rfgets(char *s, int maxCount, RFILE *stream)
{
	if (!stream)
		return NULL;
	return fgets(s, maxCount, stream);
}

RB_KEEP void filestream_vfs_init(const struct retro_vfs_interface_info *vfs_info)
{
	(void)vfs_info;
}

RB_KEEP RFILE *filestream_open(const char *path, unsigned mode, unsigned hints)
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

RB_KEEP int filestream_close(RFILE *stream)
{
	return rfclose(stream);
}

RB_KEEP int64_t filestream_tell(RFILE *stream)
{
	return rftell(stream);
}

RB_KEEP int64_t filestream_seek(RFILE *stream, int64_t offset, int seek_position)
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

RB_KEEP int64_t filestream_read(RFILE *stream, void *data, int64_t len)
{
	if (!stream || len <= 0)
		return 0;
	return (int64_t)fread(data, 1, (size_t)len, stream);
}

RB_KEEP int64_t filestream_write(RFILE *stream, const void *data, int64_t len)
{
	if (!stream || len <= 0)
		return 0;
	return (int64_t)fwrite(data, 1, (size_t)len, stream);
}

RB_KEEP char *filestream_gets(RFILE *stream, char *s, size_t len)
{
	if (!stream || !s || !len)
		return NULL;
	return fgets(s, (int)len, stream);
}

RB_KEEP size_t strlcpy_retro__(char *dest, const char *source, size_t size)
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

RB_KEEP size_t strlcat_retro__(char *dest, const char *source, size_t size)
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

RB_KEEP size_t fill_pathname_join(char *out_path, const char *dir, const char *path, size_t size)
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

#if defined(__EMSCRIPTEN__) && defined(ROMBUNDLER_WASM_DYNAMIC)
/* snes9x2010 (STATIC_LINKING) imports these from the frontend. A missing
 * one is the "resolved is not a function" stub crash on the first call. */

RB_KEEP int filestream_getc(RFILE *stream)
{
	if (!stream)
		return EOF;
	return fgetc(stream);
}

RB_KEEP int filestream_eof(RFILE *stream)
{
	if (!stream)
		return 1;
	return feof(stream);
}

RB_KEEP int filestream_printf(RFILE *stream, const char *format, ...)
{
	va_list ap;
	int n;

	if (!stream || !format)
		return -1;
	va_start(ap, format);
	n = vfprintf(stream, format, ap);
	va_end(ap);
	return n;
}

RB_KEEP void *memalign_alloc(size_t boundary, size_t len)
{
	void **place;
	uintptr_t addr;
	void *ptr;

	if (boundary < sizeof(uintptr_t))
		boundary = sizeof(uintptr_t);
	ptr = malloc(boundary + len + sizeof(uintptr_t));
	if (!ptr)
		return NULL;
	addr = ((uintptr_t)ptr + sizeof(uintptr_t) + boundary) & ~(boundary - 1);
	place = (void **)addr;
	place[-1] = ptr;
	return (void *)addr;
}

RB_KEEP void memalign_free(void *ptr)
{
	void **place;

	if (!ptr)
		return;
	place = (void **)ptr;
	free(place[-1]);
}

RB_KEEP void *memalign_alloc_aligned(size_t len)
{
#if defined(__x86_64__) || defined(__LP64__) || defined(__IA64__) || defined(_M_X64) || defined(_WIN64)
	return memalign_alloc(64, len);
#else
	return memalign_alloc(32, len);
#endif
}

RB_KEEP unsigned cpu_features_get_core_amount(void)
{
	return 1;
}

RB_KEEP uint32_t encoding_crc32(uint32_t crc, const uint8_t *buf, size_t len)
{
	if (!buf || !len)
		return crc;
	return (uint32_t)crc32(crc, buf, (uInt)len);
}

RB_KEEP int rpng_save_image_argb(const char *path, const uint32_t *data,
	unsigned width, unsigned height, unsigned pitch)
{
	(void)path;
	(void)data;
	(void)width;
	(void)height;
	(void)pitch;
	return 0;
}

RB_KEEP void *image_transfer_new(int type)
{
	(void)type;
	return NULL;
}

RB_KEEP void image_transfer_free(void *data, int type)
{
	(void)data;
	(void)type;
}

RB_KEEP void image_transfer_set_buffer_ptr(void *data, int type, void *ptr, size_t len)
{
	(void)data;
	(void)type;
	(void)ptr;
	(void)len;
}

RB_KEEP int image_transfer_start(void *data, int type)
{
	(void)data;
	(void)type;
	return 0;
}

RB_KEEP int image_transfer_iterate(void *data, int type)
{
	(void)data;
	(void)type;
	return 0;
}

RB_KEEP int image_transfer_is_valid(void *data, int type)
{
	(void)data;
	(void)type;
	return 0;
}

RB_KEEP int image_transfer_process(void *data, int type, uint32_t **buf, size_t len,
	unsigned *width, unsigned *height, int supports_rgba)
{
	(void)data;
	(void)type;
	(void)buf;
	(void)len;
	(void)width;
	(void)height;
	(void)supports_rgba;
	return 0;
}

struct rb_inflate {
	z_stream zs;
	int ready;
};

RB_KEEP void *rinflate_new(int window_bits)
{
	struct rb_inflate *stream;

	if (window_bits == 0)
		window_bits = 15;
	stream = (struct rb_inflate *)calloc(1, sizeof(*stream));
	if (!stream)
		return NULL;
	if (inflateInit2(&stream->zs, window_bits) != Z_OK) {
		free(stream);
		return NULL;
	}
	stream->ready = 1;
	return stream;
}

RB_KEEP void rinflate_free(void *handle)
{
	struct rb_inflate *stream = (struct rb_inflate *)handle;

	if (!stream)
		return;
	if (stream->ready)
		inflateEnd(&stream->zs);
	free(stream);
}

RB_KEEP void rinflate_set_in(void *handle, const uint8_t *in, size_t in_size)
{
	struct rb_inflate *stream = (struct rb_inflate *)handle;

	if (!stream)
		return;
	stream->zs.next_in = (Bytef *)in;
	stream->zs.avail_in = (uInt)in_size;
}

RB_KEEP void rinflate_set_out(void *handle, uint8_t *out, size_t out_size)
{
	struct rb_inflate *stream = (struct rb_inflate *)handle;

	if (!stream)
		return;
	stream->zs.next_out = out;
	stream->zs.avail_out = (uInt)out_size;
}

RB_KEEP int rinflate_process(void *handle, size_t *read, size_t *wrote)
{
	struct rb_inflate *stream = (struct rb_inflate *)handle;
	uInt in_before;
	uInt out_before;
	int ret;

	if (!stream || !stream->ready)
		return -2;
	in_before = stream->zs.avail_in;
	out_before = stream->zs.avail_out;
	ret = inflate(&stream->zs, Z_NO_FLUSH);
	if (read)
		*read = (size_t)(in_before - stream->zs.avail_in);
	if (wrote)
		*wrote = (size_t)(out_before - stream->zs.avail_out);
	if (ret == Z_STREAM_END)
		return 1;
	if (ret == Z_OK || ret == Z_BUF_ERROR)
		return 0;
	return -2;
}
#endif
