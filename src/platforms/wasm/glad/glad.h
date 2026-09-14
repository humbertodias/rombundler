#ifndef ROMBUNDLER_WASM_GLAD_H
#define ROMBUNDLER_WASM_GLAD_H

/* video.c includes <glad/glad.h>. Emscripten provides GLES3. */
#include <GLES3/gl3.h>

/* Desktop-only tokens referenced in video.c behind platform_gles()==0. */
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_UNSIGNED_INT_8_8_8_8_REV
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#endif
#ifndef GL_RGB565
#define GL_RGB565 0x8D62
#endif
#ifndef GL_RGB5_A1
#define GL_RGB5_A1 0x8057
#endif

typedef void *(*GLADloadproc)(const char *name);

static inline int gladLoadGLLoader(GLADloadproc load)
{
	(void)load;
	return 1;
}

#endif
