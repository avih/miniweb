#define SHELL_ALLOC 0x1
#define SHELL_SHOW_WINDOW 0x2
#define SHELL_REDIRECT_STDIN 0x4
#define SHELL_REDIRECT_STDOUT 0x8

typedef struct {
	char *pchCommandLine;
	char *pchCurDir;
	char *pchPath;
#ifdef WIN32
	HANDLE fdRead;
	HANDLE fdWrite;
	PROCESS_INFORMATION piProcInfo;
#else
	int fdRead;
	int fdWrite;
	int pid;
#endif
	char *buffer;
	int iBufferSize;
	int iDelimiter;
	unsigned int flags;
}SHELL_PARAM;

#ifdef __cplusplus
extern "C" {
#endif
int ShellRead(SHELL_PARAM* param);
void ShellClean(SHELL_PARAM* param);
int ShellWait(SHELL_PARAM* param, int iTimeout);
int ShellExec(SHELL_PARAM* param);
int ShellTerminate(SHELL_PARAM* param);
int ShellSetPriority(SHELL_PARAM* param, int iPriority);
#ifdef __cplusplus
}
#endif
