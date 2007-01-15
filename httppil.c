/////////////////////////////////////////////////////////////////////////////
//
// httppil.c
//
// MiniWeb Platform Independent Layer
//
/////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <io.h>
#include "httppil.h"

#ifndef WIN32
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#endif

int InitSocket()
{
#ifdef WIN32
	WSADATA wsaData;
	if ( WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) ) {
		return 0;
	}
#endif
	return 1;   
}

void UninitSocket()
{
#ifdef WIN32
  WSACleanup( );
#endif
}

char *GetTimeString()
{
	static char buf[16];
	time_t tm=time(NULL);
	memcpy(buf,ctime(&tm)+4,15);
	buf[15]=0;
	return buf;
}

#ifndef NOTHREAD
int ThreadCreate(pthread_t *pth, void* (*start_routine)(void*), void* arg)
{
#ifdef WIN32
	DWORD dwid;	    
	*pth=CreateThread(0,0,(LPTHREAD_START_ROUTINE)start_routine,arg,0,&dwid);
	return *pth!=NULL?0:1;
#else
	return pthread_create(pth,NULL,start_routine, arg);
#endif
}

int ThreadKill(pthread_t pth)
{
#ifdef WIN32
	return TerminateThread(pth,0)?0:1;
#else
	return pthread_cancel(pth);
#endif
}

int ThreadWait(pthread_t pth,void** ret)
{
#ifdef WIN32
	if (WaitForSingleObject(pth,INFINITE)!=WAIT_OBJECT_0)
		return GetLastError();
	if (ret) GetExitCodeThread(pth,(LPDWORD)ret);
	return 0;
#else
	return pthread_join(pth,ret);
#endif
}

void MutexCreate(pthread_mutex_t* mutex)
{
#ifdef WIN32
	CreateMutex(0,FALSE,NULL);
#else
	pthread_mutex_init(mutex,NULL);
#endif
}

void MutexDestroy(pthread_mutex_t* mutex)
{
#ifdef WIN32
	CloseHandle(*mutex);
#else
	pthread_mutex_destroy(mutex);
#endif
}

void MutexLock(pthread_mutex_t* mutex)
{
#ifdef WIN32
	WaitForSingleObject(*mutex,INFINITE);
#else
	pthread_mutex_lock(mutex);
#endif
}

void MutexUnlock(pthread_mutex_t* mutex)
{
#ifdef WIN32
	ReleaseMutex(*mutex);
#else
	pthread_mutex_unlock(mutex);
#endif
}

#endif

int IsDir(char* pchName)
{
#ifdef WIN32
	DWORD attr=GetFileAttributes(pchName);
	//if (attr==INVALID_FILE_ATTRIBUTES) return 0;
	return (attr & FILE_ATTRIBUTE_DIRECTORY)?1:0;
#else
	struct stat stDirInfo;
	if (stat( pchName, &stDirInfo) < 0) return 0;
	return (stDirInfo.st_mode & S_IFDIR)?1:0;
#endif //WIN32
}

int ReadDir(char* pchDir, char* pchFileNameBuf)
{
#ifdef WIN32
	static HANDLE hFind=NULL;
	WIN32_FIND_DATA finddata;

	if (!pchFileNameBuf) {
		if (hFind) {
			FindClose(hFind);
			hFind=NULL;
		}
		return 0;
	}
	if (pchDir) {
		char *p;
		if (!IsDir(pchDir)) return -1;
		if (hFind) FindClose(hFind);
		p = malloc(strlen(pchDir) + 5);
		sprintf(p, "%s\\*.*", pchDir);
		hFind=FindFirstFile(p,&finddata);
		free(p);
		if (hFind==INVALID_HANDLE_VALUE) {
			hFind=NULL;
			return -1;
		}
		strcpy(pchFileNameBuf,finddata.cFileName);
		return 0;
	}
	if (!hFind) return -1;
	if (!FindNextFile(hFind,&finddata)) {
		FindClose(hFind);
		hFind=NULL;
		return -1;
	}
	strcpy(pchFileNameBuf,finddata.cFileName);
#else
	static DIR *stDirIn=NULL;
	struct dirent *stFiles;

	if (!pchFileNameBuf) {
		if (stDirIn) {
			closedir(stDirIn);
			stDirIn=NULL;
		}
		return 0;
	}
	if (pchDir) {
		if (!IsDir(pchDir)) return -1;
		if (stDirIn) closedir(stDirIn);
		stDirIn = opendir( pchDir);
	}
	if (!stDirIn) return -1;
	stFiles = readdir(stDirIn);
	if (!stFiles) {
		closedir(stDirIn);
		stDirIn=NULL;
		return -1;
	}
	strcpy(pchFileNameBuf, stFiles->d_name);
#endif
	return 0;
}

int IsFileExist(char* filename)
{
	int fd = open(filename,O_RDONLY);
	if (fd>0) {
		close(fd);
		return 1;
	}
	return 0;
}
