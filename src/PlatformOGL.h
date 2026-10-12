#ifndef PLATFORMOGL_H
#define PLATFORMOGL_H

#ifndef MELONDS_GL_HEADER
#define MELONDS_GL_HEADER "\"frontend/glad/glad.h\""
#endif

#include MELONDS_GL_HEADER

#ifdef MELONDS_GLES
    // Compatibilidad GLES: renombrar funciones/constantes de desktop
    #ifndef GL_UNSIGNED_SHORT_1_5_5_5_REV
        #define GL_UNSIGNED_SHORT_1_5_5_5_REV GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT
    #endif
    #ifndef glDepthRange
        #define glDepthRange glDepthRangef
    #endif
    #ifndef glClearDepth
        #define glClearDepth glClearDepthf
    #endif
    #ifndef glBindFragDataLocation
        #define glBindFragDataLocation glBindFragDataLocationEXT
    #endif

    // glDrawBuffer: en GLES solo existe glDrawBuffers (plural)
    static inline void glDrawBuffer_compat(GLenum buf) {
        glDrawBuffers(1, &buf);
    }
    #define glDrawBuffer glDrawBuffer_compat

    // glMapBuffer: en GLES 3.0+ solo existe glMapBufferRange
    static inline void* glMapBuffer_compat(GLenum target, GLenum access) {
        GLint size = 0;
        glGetBufferParameteriv(target, GL_BUFFER_SIZE, &size);
        GLbitfield flags = 0;
        if (access == GL_WRITE_ONLY) flags = GL_MAP_WRITE_BIT;
        else if (access == GL_READ_ONLY) flags = GL_MAP_READ_BIT;
        else flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
        return glMapBufferRange(target, 0, size, flags);
    }
    #define glMapBuffer glMapBuffer_compat
#endif

#endif
