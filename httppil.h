///////////////////////////////////////////////////////////////////////
//
// httpapi.h
//
// Header file for Miniweb Platform Independent Layer
//
///////////////////////////////////////////////////////////////////////

#ifndef _HTTPPIL_H_
#define _HTTPPIL_H_

#ifdef SYS_MINGW
#ifndef WIN32
#define WIN32
#endif
#endif

#ifdef WIN32
#include <windows.h>
#include <io.h>

#ifndef SYS_MINGW
#define read _read
#define open _open
#define close _close
#define lseek _lseek
#define read _read
#define write _write
#define strdup _strdup
#define dup2 _dup2
#define dup _dup
#define pipe _pipe
#define spawnvpe _spawnvpe
#define spawnvp _spawnvp
#define snprintf _snprintf
#else
#include <winsock2.h>
#endif

#else
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#endif

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

#ifdef WIN32

#define ssize_t size_t
#define socklen_t int
#ifndef HAVE_PTHREAD
typedef HANDLE pthread_t;
typedef HANDLE pthread_mutex_t;
#endif

typedef DWORD (WINAPI *PFNGetProcessId)(HANDLE hProcess);

#else

#define closesocket close
#define MAX_PATH 256
#define FALSE 0
#define TRUE 1

typedef int SOCKET;
typedef unsigned long DWORD;
typedef unsigned short int WORD;
typedef unsigned char BYTE;
typedef int BOOL;
#endif
typedef unsigned char OCTET;

#define SHELL_NOREDIRECT 1
#define SHELL_SHOWWINDOW 2
#define SHELL_NOWAIT 4

#ifdef WIN32
#define msleep(ms) (Sleep(ms))
#else
#define msleep(ms) (usleep(ms<<10))
#endif


#ifdef __cplusplus
extern "C" {
#endif

int InitSocket();
void UninitSocket();
char *GetTimeString();
int ThreadCreate(pthread_t *pth, void* (*start_routine)(void*), void* arg);
int ThreadKill(pthread_t pth);
int ThreadWait(pthread_t pth,void** ret);
void MutexCreate(pthread_mutex_t* mutex);
void MutexDestroy(pthread_mutex_t* mutex);
void MutexLock(pthread_mutex_t* mutex);
void MutexUnlock(pthread_mutex_t* mutex);
int ReadDir(char* pchDir, char* pchFileNameBuf);
int IsFileExist(char* filename);
int IsDir(char* pchName);

#ifdef __cplusplus
}
#endif

#endif
