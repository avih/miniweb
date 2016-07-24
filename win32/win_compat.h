/*******************************************************************************
*  Win32 utf8 wrappers
*  Copyright (C) 2015 Avi Halachmi
*
*  This program is free software; you can redistribute it and/or
*  modify it under the terms of the GNU General Public License
*  as published by the Free Software Foundation; either version 2
*  of the License, or (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*******************************************************************************/

#ifndef _CC_WIN_COMPAT_H
#define _CC_WIN_COMPAT_H


/**
 * - If not Windows or #ifdef CC_DISABLE_WIN_UTF8: all cc_ABC are pretty much ABC
 *   except that on windows cc_fseeko maps to fseek, and cc_ftello maps to ftell
 *   since there are no native 'o' variants.
 *
 *         If windows and not disabled via CC_DISABLE_WIN_UTF8:
 *
 * - Use "argv = cc_get_argvutf8(argc, argv, &success);" to convert argv to utf8.
 *   On failure, argv remains the same. On success, use cc_free_argvutf8 when done.
 *   cc_*_argvutf8 don't need #ifdef WIN32, but are only effective on windows when
 *   not disabled via CC_DISABLE_WIN_UTF8.
 *
 * - cc_USIZE(n) just multiples n (if WIN32) to accommodate longer strings as utf8.
 *
 * - All cc_<file-related> use 64 bits sizes, including CC_OFF_T_MIN[/MAX].
 *   Note: use struct cc_stat_s for struct stat, and cc_[f]stat(..) for [f]stat(..)
 *
 * - All char* arguments can be utf8 (or ansi): fopen, open, stat, fprintf,
 *   and FindFirst[/Next]File (win only - use cc_WIN32_FIND_DATA which has char*).
 *   Internally they're converted to wide chars and use the W win32 APIs,
 *   Except cc_fprintf - which converts to wide chars only if stream is a tty.
 */

#if defined(WIN32) || defined(_WIN32)
    #define CC_WIN32
#endif

#if !defined(CC_WIN32) || defined(CC_DISABLE_WIN_UTF8)
    #define CC_OFF_T_MIN  OFF_T_MIN
    #define CC_OFF_T_MAX  OFF_T_MAX
    #define cc_off_t    off_t
    #define cc_fopen    fopen
    #define cc_open     open
    #define cc_fprintf  fprintf
    #define cc_stat_s   stat
    #define cc_stat     stat
    #define cc_fstat    fstat
    #define cc_main     main

    #define cc_USIZE(n) (n)
    #define cc_get_argvutf8(argc, argv, pOK) ((*(pOK) = 0), (argv))
    #define cc_free_argvutf8(argc, argv)  /* noop */

    #ifdef CC_WIN32
        #define cc_fseeko fseek
        #define cc_ftello ftell

        #define cc_FindFirstFile   FindFirstFile
        #define cc_FindNextFile    FindNextFile
        #define cc_WIN32_FIND_DATA WIN32_FIND_DATA
    #else
        #define cc_fseeko fseeko
        #define cc_ftello ftello
    #endif
#else

/* utf8 size, allocate 4 byes for each char */
#define cc_USIZE(n) (4 * (n))

// Windows fugliness.
// For unicode support (enabled by default): link with shell32.lib.
//   E.g. with msvc add: shell32.lib, with gcc/tcc add: -lshell32
// To disable utf8 support, add (msvc) -DCC_DISABLE_WIN_UTF8

// MSVC - suppress warnings which we don't need:
// - fopen doesn't check for null values, we do.
#define _CRT_SECURE_NO_WARNINGS

// mingw - support %lld in printf without warning. This adds a small bulk of
// ansi formatting support to the exe and makes sure it always works.
#define __USE_MINGW_ANSI_STDIO 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

// Some windows compilers (mingw) can support off_t, ftello, etc, but they
// still have to map those to the actual windows API, so use this API
// directly - less things to go wrong (64) and support other compilers too.
#include <io.h>
#include <fcntl.h>
#define cc_off_t     __int64
#define cc_fseeko    _fseeki64
// _ftelli64 is hard to link. _telli64 + _fileno is the same, easier to link
#define cc_ftello(fd) _telli64(_fileno(fd))

#define CC_OFF_T_MIN _I64_MIN
#define CC_OFF_T_MAX _I64_MAX

#define CC_HAVE_WIN_UTF8

#include <windows.h>
#ifndef __TINYC__
	#include <shellapi.h>
#else
	// tcc 0.26 (latest) doesn't have shellapi.h, needs this (remember -lshell32)
	LPWSTR * __stdcall CommandLineToArgvW(LPCWSTR, int *);
#endif

// utf8 and win console code from mpv, modified to:
// - Use malloc instead of talloc.
// - Don't care about ansi escaping.
// - https://github.com/mpv-player/mpv/blob/master/osdep/io.c#L98
// - https://github.com/mpv-player/mpv/blob/master/osdep/terminal-win.c#L170

/*
 * unicode/utf-8 I/O helpers and wrappers for Windows
 *
 * Contains parts based on libav code (http://libav.org).
 *
 * This file is part of mpv.
 *
 * mpv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with mpv.  If not, see <http://www.gnu.org/licenses/>.
 */

static wchar_t *mp_from_utf8(const char *s)
{
    int count = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (count <= 0)
        return NULL;
    wchar_t *ret = malloc(sizeof(wchar_t) * (count + 1));
    MultiByteToWideChar(CP_UTF8, 0, s, -1, ret, count);
    return ret;
}

static char *mp_to_utf8(const wchar_t *s)
{
    int count = WideCharToMultiByte(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL);
    if (count <= 0)
        return NULL;
    char *ret = malloc(sizeof(char) * count);
    WideCharToMultiByte(CP_UTF8, 0, s, -1, ret, count, NULL, NULL);
    return ret;
}

static HANDLE cc_get_out_console_handle(FILE *f)
{
    DWORD dummy;
    HANDLE h;
    int fd = _fileno(f);

    if (_isatty(fd) &&
        INVALID_HANDLE_VALUE != (h = (HANDLE)_get_osfhandle(fd)) &&
        GetConsoleMode(h, &dummy))
    {
        return h;
    } else {
        return INVALID_HANDLE_VALUE;
    }
}

static void write_console_text(HANDLE wstream, char *buf)
{
    wchar_t *out = mp_from_utf8(buf);
    size_t out_len = wcslen(out);
    WriteConsoleW(wstream, out, out_len, NULL, NULL);
    free(out);
}

// if f is a tty, prints wide chars, else utf8.
// originally used mp_write_console_ansi which also translates ansi sequences,
// but we don't need it so use plain write_console_text instead.
static int mp_vfprintf(FILE *f, const char *format, va_list args)
{
    int rv = 0;
    HANDLE h = cc_get_out_console_handle(f);

    if (h != INVALID_HANDLE_VALUE) {
        size_t len = vsnprintf(NULL, 0, format, args) + 1;
        char *buf = malloc(sizeof(char) * len);

        if (buf) {
            rv = vsnprintf(buf, len, format, args);
            write_console_text(h, buf);
        }
        free(buf);
    } else {
        rv = vfprintf(f, format, args);
    }

    return rv;
}
// ------------------- end of mpv code --------------------------------


// on success, returns a utf8 argv and sets out_success to 1. On failure returns
// the original argv. if success, caller needs to free with cc_free_argvutf8
static char **cc_get_argvutf8(int argc_validation, char **argv_orig, int *out_success)
{
    *out_success = 0;
    int nArgs;
    LPWSTR *szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
    if (!szArglist || nArgs != argc_validation) {
        if (szArglist)
            LocalFree(szArglist);
        return argv_orig;
    }

    char **argvu = malloc(sizeof(char*) * (nArgs + 1));
    int i;
    for (i = 0; i < nArgs; i++) {
        argvu[i] = mp_to_utf8(szArglist[i]);
        if (!argvu[i])
            return argv_orig; // leaking the previous strings. we don't care.
    }
    argvu[nArgs] = NULL;

    LocalFree(szArglist);
    *out_success = 1;
    return argvu;
}

static void cc_free_argvutf8(int argc, char** argvu)
{
    int i;
    for (i = 0; i < argc; i++)
        free(argvu[i]);
    free(argvu);
}

#define cc_main(...)                             \
/* int already here */ _cc_main(int, char**);    \
int main(int argc, char **argv) {                \
    int argvok = 0, rv;                          \
    argv = cc_get_argvutf8(argc, argv, &argvok); \
    rv = _cc_main(argc, argv);                   \
    if (argvok) cc_free_argvutf8(argc, argv);    \
    return rv;                                   \
}                                                \
int _cc_main(__VA_ARGS__)  /* user's main body should follow */

static int cc_fprintf(FILE *stream, const char *format, ...)
{
    int res;
    va_list args;
    va_start(args, format);

    res = mp_vfprintf(stream, format, args);

    va_end(args);
    return res;
}

static FILE *cc_fopen(const char *fname, const char *mode) {
    wchar_t *wfname, *wmode;
    if (!fname || !mode) return 0;
    wfname = mp_from_utf8(fname);
    wmode  = mp_from_utf8(mode);

    FILE *rv = _wfopen(wfname, wmode);

    free(wmode);
    free(wfname);
    return rv;
}

static int cc_open(const char *fname, int oflags) {
    wchar_t *wfname;
    if (!fname) return -1;
    wfname = mp_from_utf8(fname);

    int rv = _wopen(wfname, oflags);

    free(wfname);
    return rv;
}

#define _CFN_SIZE cc_USIZE(MAX_PATH)
#define _CAFN_SIZE cc_USIZE(14)
/* Identical to WIN32_FIND_DATAA (two A) except bigger char* buffers for utf8 */
typedef struct _cc_WIN32_FIND_DATA {
    DWORD    dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD    nFileSizeHigh;
    DWORD    nFileSizeLow;
    DWORD    dwReserved0;
    DWORD    dwReserved1;
    char    cFileName[_CFN_SIZE];
    char    cAlternateFileName[_CAFN_SIZE];
} cc_WIN32_FIND_DATA;

#define _cccpfd(a, b, attr) (a)->attr = (b)->attr;
static void _cc_wfd2ccfd(cc_WIN32_FIND_DATA *ofd, const WIN32_FIND_DATAW *wfd) {
    char *cfn, *cafn;

    _cccpfd(ofd, wfd, dwFileAttributes);
    _cccpfd(ofd, wfd, ftCreationTime);
    _cccpfd(ofd, wfd, ftLastAccessTime);
    _cccpfd(ofd, wfd, ftLastWriteTime);
    _cccpfd(ofd, wfd, nFileSizeHigh);
    _cccpfd(ofd, wfd, nFileSizeLow);
    _cccpfd(ofd, wfd, dwReserved0);
    _cccpfd(ofd, wfd, dwReserved0);

    cfn = mp_to_utf8(wfd->cFileName);
    strncpy(ofd->cFileName, cfn, _CFN_SIZE - 1); ofd->cFileName[_CFN_SIZE - 1] = 0;
    free(cfn);

    cafn = mp_to_utf8(wfd->cAlternateFileName);
    strncpy(ofd->cAlternateFileName, cafn, _CAFN_SIZE - 1); ofd->cAlternateFileName[_CAFN_SIZE - 1] = 0;
    free(cafn);
}

static HANDLE cc_FindFirstFile(const char *path, cc_WIN32_FIND_DATA *ofd) {
    WIN32_FIND_DATAW wfd;
    HANDLE rv;

    wchar_t *wp = mp_from_utf8(path);
    rv = FindFirstFileW(wp, &wfd);
    free(wp);

    if (INVALID_HANDLE_VALUE != rv)
        _cc_wfd2ccfd(ofd, &wfd);
    return rv;
}

static BOOL cc_FindNextFile(HANDLE h, cc_WIN32_FIND_DATA *ofd) {
    WIN32_FIND_DATAW wfd;
    BOOL rv = FindNextFileW(h, &wfd);

    if (rv)
        _cc_wfd2ccfd(ofd, &wfd);
    return rv;
}

#define cc_fstat  _fstat64
#define cc_stat_s _stat64
static int cc_stat(const char *path, struct cc_stat_s *ss) {
    wchar_t *wp = mp_from_utf8(path);
    int rv = _wstat64(wp, ss);
    free(wp);
    return rv;
}


#endif /* not defined(CC_WIN32) || defined(CC_DISABLE_WIN_UTF8) */

#endif /* _CC_WIN_COMPAT_H */
