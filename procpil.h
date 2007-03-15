#ifdef WIN32
#include <process.h>
#else
#define O_NOINHERIT 0
#define O_BINARY 0
#define P_NOWAIT 0
#endif

#define READ_FD 0
#define WRITE_FD 1

#define SF_ALLOC 0x1
#define SF_LOOP_READ 0x2
#define SF_REDIRECT_STDIN 0x1000
#define SF_REDIRECT_STDOUT 0x2000
#define SF_REDIRECT_STDERR 0x4000
#define SF_REDIRECT_OUTPUT (0x8000 | SF_REDIRECT_STDOUT)

typedef struct {
	char *pchCurDir;
	char *pchPath;
	char** env;
	int fdStdoutRead;
	int fdStderrRead;
	int fdStdinWrite;
	intptr_t hproc;
	char *buffer;
	int iBufferSize;
	unsigned int flags;
}SHELL_PARAM;

#ifdef __cplusplus
extern "C" {
#endif
int ShellRead(SHELL_PARAM* param);
void ShellClean(SHELL_PARAM* param);
int ShellWait(SHELL_PARAM* param, int iTimeout);
int ShellExec(SHELL_PARAM* param, char* commandLine);
int ShellTerminate(SHELL_PARAM* param);
int ShellSetPriority(SHELL_PARAM* param, int iPriority);
#ifdef __cplusplus
}
#endif
