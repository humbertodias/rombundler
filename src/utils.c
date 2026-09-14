#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "platform.h"

void die(const char *fmt, ...) {
	char buffer[4096];

	va_list va;
	va_start(va, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, va);
	va_end(va);

	platform_fatal(buffer);
}
