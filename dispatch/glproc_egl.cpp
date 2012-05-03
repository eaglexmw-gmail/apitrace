/**************************************************************************
 *
 * Copyright 2011 Jose Fonseca
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/


#include <assert.h>

#if !defined(_WIN32)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE // for dladdr
#endif
#include <dlfcn.h>
#endif


#include "glproc.hpp"


#if defined(_WIN32)

#error Unsupported

#elif defined(__APPLE__)

#error Unsupported

#else

#if defined(_WIN32)
#define LIBGL_FILENAME "opengl32.dll"
#elif defined(__APPLE__)
// FIXME
#else
#define LIBGL_FILENAME "libGL.so.1"
#endif


static void *
_openLibrary(const char *filename)
{
#if defined(_WIN32) || defined(__APPLE__) || defined(ANDROID)

    return os::openLibrary(filename);

#else

    /*
     * Invoke the true dlopen() function.
     */

    typedef void * (*PFNDLOPEN)(const char *, int);
    static PFNDLOPEN dlopen_ptr = NULL;

    if (!dlopen_ptr) {
        dlopen_ptr = (PFNDLOPEN)dlsym(RTLD_NEXT, "dlopen");
        if (!dlopen_ptr) {
            os::log("apitrace: error: dlsym(RTLD_NEXT, \"dlopen\") failed\n");
            return NULL;
        }
    }

    return dlopen_ptr(filename, RTLD_LOCAL | RTLD_LAZY);

#endif
}


static os::Library
_getPublicProcLibrary(const char *procName)
{
    using namespace gldispatch;

    if (procName[0] == 'e' && procName[1] == 'g' && procName[2] == 'l') {
        if (!gldispatch::libEGL) {
            libEGL = _openLibrary("libEGL" OS_LIBRARY_EXTENSION);
        }
        return libEGL;
    }

    Profile profile = currentProfile;

    if ((procName[0] == 'g' && procName[1] == 'l' && procName[2] == 'X') ||
        (procName[0] == 'w' && procName[1] == 'g' && procName[2] == 'l') ||
        (procName[0] == 'C' && procName[1] == 'G' && procName[2] == 'L')) {
        profile = PROFILE_COMPAT;
    }

    switch (profile) {
    case PROFILE_COMPAT:
    case PROFILE_CORE:
        if (!libGL) {
            libGL = _openLibrary(LIBGL_FILENAME);
        }
        return libGL;
    case PROFILE_ES1:
        if (!libGLESv1) {
            libGLESv1 = _openLibrary("libGLESv1_CM" OS_LIBRARY_EXTENSION);
        }
        return libGLESv1;
    case PROFILE_ES2:
        if (!libGLESv2) {
            libGLESv2 = _openLibrary("libGLESv2" OS_LIBRARY_EXTENSION);
        }
        return libGLESv2;
    default:
        assert(0);
        return NULL;
    }

    return NULL;
}


/*
 * Lookup a public EGL/GL/GLES symbol
 *
 * The spec states that eglGetProcAddress should only be used for non-core
 * (extensions) entry-points.  Core entry-points should be taken directly from
 * the API specific libraries.
 *
 * We cannot tell here which API a symbol is meant for here (as some are
 * exported by many).  So this code assumes that the appropriate shared
 * libraries have been loaded previously (either dlopened with RTLD_GLOBAL, or
 * as part of the executable dependencies), and that their symbols available
 * for quering via dlsym(RTLD_NEXT, ...).
 *
 * Android does not support LD_PRELOAD.  It is assumed that applications
 * are explicitely loading egltrace.so.
 */
void *
_getPublicProcAddress(const char *procName)
{
    os::Library lib;
    void *sym = NULL;

    lib = _getPublicProcLibrary(procName);
    if (lib) {
        sym = os::getLibrarySymbol(lib, procName);
    }

    return sym;
}

/*
 * Lookup a private EGL/GL/GLES symbol
 *
 * Private symbols should always be available through eglGetProcAddress, and
 * they are guaranteed to work with any context bound (regardless of the API).
 *
 * However, per issue#57, eglGetProcAddress returns garbage on some
 * implementations, and the spec states that implementations may choose to also
 * export extension functions publicly, so we always attempt dlsym before
 * eglGetProcAddress to mitigate that.
 */
void *
_getPrivateProcAddress(const char *procName)
{
    void *proc;
    proc = _getPublicProcAddress(procName);
    if (!proc &&
        ((procName[0] == 'e' && procName[1] == 'g' && procName[2] == 'l') ||
         (procName[0] == 'g' && procName[1] == 'l'))) {
        proc = (void *) _eglGetProcAddress(procName);
    }

    return proc;
}

#endif
