#ifndef _GLITCH_WGL_H
#define _GLITCH_WGL_H

#include <windows.h>
extern "C" {
    #include <SDL_opengl.h>
    extern PFNGLACTIVETEXTUREARBPROC glActiveTextureARB;
    extern PFNGLATTACHOBJECTARBPROC glAttachObjectARB;
    extern PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT;
    extern PFNGLBINDRENDERBUFFEREXTPROC glBindRenderbufferEXT;
    extern PFNGLBLENDFUNCSEPARATEEXTPROC glBlendFuncSeparateEXT;
    extern PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC glCheckFramebufferStatusEXT;
    extern PFNGLCOMPILESHADERARBPROC glCompileShaderARB;
    extern PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB;
    extern PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB;
    extern PFNGLDELETERENDERBUFFERSEXTPROC glDeleteRenderbuffersEXT;
    extern PFNGLDELETEFRAMEBUFFERSEXTPROC glDeleteFramebuffersEXT;
    extern PFNGLFOGCOORDFEXTPROC glFogCoordfEXT;
    extern PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbufferEXT;
    extern PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glFramebufferTexture2DEXT;
    extern PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT;
    extern PFNGLGENRENDERBUFFERSEXTPROC glGenRenderbuffersEXT;
    extern PFNGLGETINFOLOGARBPROC glGetInfoLogARB;
    extern PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB;
    extern PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB;
    extern PFNGLLINKPROGRAMARBPROC glLinkProgramARB;
    extern PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB;
    extern PFNGLRENDERBUFFERSTORAGEEXTPROC glRenderbufferStorageEXT;
    extern PFNGLSECONDARYCOLOR3FPROC glSecondaryColor3f;
    extern PFNGLSHADERSOURCEARBPROC glShaderSourceARB;
    extern PFNGLUNIFORM1FARBPROC glUniform1fARB;
    extern PFNGLUNIFORM1IARBPROC glUniform1iARB;
    extern PFNGLUNIFORM4FARBPROC glUniform4fARB;
    extern PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB;
    extern PFNGLGETHANDLEARBPROC glGetHandleARB;
    typedef const char * (WINAPI * PFNWGLGETEXTENSIONSSTRINGARBPROC) (HDC hdc);
}

extern PFNGLACTIVETEXTUREARBPROC glActiveTextureARB;
extern PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB;
extern PFNGLBLENDFUNCSEPARATEEXTPROC glBlendFuncSeparateEXT;
extern PFNGLFOGCOORDFPROC glFogCoordfEXT;

extern PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB;
extern PFNGLSHADERSOURCEARBPROC glShaderSourceARB;
extern PFNGLCOMPILESHADERARBPROC glCompileShaderARB;
extern PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB;
extern PFNGLATTACHOBJECTARBPROC glAttachObjectARB;
extern PFNGLLINKPROGRAMARBPROC glLinkProgramARB;
extern PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB;
extern PFNGLGETHANDLEARBPROC glGetHandleARB;
extern PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB;
extern PFNGLUNIFORM1IARBPROC glUniform1iARB;
extern PFNGLUNIFORM4IARBPROC glUniform4iARB;
extern PFNGLUNIFORM4FARBPROC glUniform4fARB;
extern PFNGLUNIFORM1FARBPROC glUniform1fARB;
extern PFNGLDELETEOBJECTARBPROC glDeleteObjectARB;
extern PFNGLGETINFOLOGARBPROC glGetInfoLogARB;
extern PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB;
extern PFNGLSECONDARYCOLOR3FPROC glSecondaryColor3f;

int isWglExtensionSupported(const char *extension);

int WGL_LookupSymbols(void);

void wgl_init(HWND hWnd);

void wgl_deinit(unsigned long fullscreen);

#endif
