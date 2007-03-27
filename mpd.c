#ifdef _MPD
#include "httppil.h"
#include "httpapi.h"
#include "procpil.h"

int mpState=0;
static SHELL_PARAM mpx;

void __stdcall mpThread()
{
	SHELL_PARAM sp;

}

int mpRead(char* buf, int bufsize)
{
	int n;
	mpx.buffer = buf;
	mpx.iBufferSize = bufsize;
	n = ShellRead(&mpx);
	if (n < 0) return -1;
	buf[n] = 0;
	return n;
}

int mpCommand(char* cmd)
{
	int ret=write(mpx.fdStdinWrite,cmd,strlen(cmd));
	write(mpx.fdStdinWrite,"\n",1);
	if (!strcmp(cmd, "quit")) {
		ShellClean(&mpx);
	}
	return ret;
}

void mpClose()
{
	if (mpState) {
		mpCommand("quit");
		ShellWait(&mpx,1);
		ShellClean(&mpx);
	}
}

int mpOpen(char* pchFilename, char* opts)
{
	char buf[512];
	mpClose();
#ifdef WIN32
	sprintf(buf,"mplayer \"%s\" -slave -quiet %s",pchFilename, opts);
#else
	sprintf(buf,"/usr/bin/mplayer %s -slave -quiet %s",pchFilename, opts);
#endif
	printf("MPlayer command line:\n%s\n", buf);
	mpx.flags = SF_REDIRECT_STDIN | SF_REDIRECT_STDOUT;
	if (ShellExec(&mpx, buf)) return -1;
	mpState=1;
	return 0;
}

int ehMpd(MW_EVENT event, int argi, void* argp)
{
	switch (event) {
	case MW_INIT:
		memset(&mpx,0,sizeof(mpx));
		break;
	case MW_UNINIT:
		mpClose();
		break;
	}
	return 0;
}

int uhMpd(UrlHandlerParam* param)
{
	char *action;
	mwParseQueryString(param);
	action = mwGetVarValue(param->pxVars, "action");
	if (!strcmp(action, "exec")) {
		char *filename = mwGetVarValue(param->pxVars, "file");
		char *args = mwGetVarValue(param->pxVars, "args");
		if (filename) mwDecodeString(filename);
		if (args) mwDecodeString(args);
		if (!mpOpen(filename, args)) {
			char *p = NULL;
			int n;
			int offset = 0;
			while (offset < param->iDataBytes) {
				n = mpRead(param->pucBuffer + offset, param->iDataBytes - offset);
				if (n <= 0) break;
				offset += n;
				if (p = strstr(param->pucBuffer, "Starting playback...")) break;
			}
			mpCommand("get_time_length");
			while (offset < param->iDataBytes) {
				n = mpRead(param->pucBuffer + offset, param->iDataBytes - offset);
				if (n <= 0) break;
				offset += n;
				if (p = strstr(param->pucBuffer, "ANS_LENGTH=")) break;
			}
			if (!p) mpState = 0;
		}
	} else if (!strcmp(action, "command")) {
		char *cmd = mwGetVarValue(param->pxVars, "command");
		if (cmd) {
			mpCommand(cmd);
			if (mpRead(param->pucBuffer, param->iDataBytes) <= 0)
				strcpy(param->pucBuffer, "Error");
		}
	} else {
		return 0;
	}
	param->iDataBytes=strlen(param->pucBuffer);
	param->fileType=HTTPFILETYPE_TEXT;
	return FLAG_DATA_RAW;
}

#endif
