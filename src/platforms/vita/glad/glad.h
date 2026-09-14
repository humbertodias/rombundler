#ifndef ROMBUNDLER_VITA_GLAD_H
#define ROMBUNDLER_VITA_GLAD_H

/* video.c includes <glad/glad.h>. vitaGL exports GL symbols directly. */
#include <vitaGL.h>

#ifndef GL_RGB565
#define GL_RGB565 0x8D62
#endif

typedef void *(*GLADloadproc)(const char *name);

static inline int gladLoadGLLoader(GLADloadproc load)
{
	(void)load;
	return 1;
}

/* GLES / vitaGL has no glValidateProgram; video.c only uses it after glLinkProgram. */
#ifndef glValidateProgram
static inline void glValidateProgram(GLuint program)
{
	(void)program;
}
#endif

#endif
