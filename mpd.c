#ifdef _MPD
#include "httppil.h"
#include "httpapi.h"
#include "procpil.h"

int mpState=0;
static SHELL_PARAM mpx;

int mpRead(char* buf, int bufsize)
{
	int bytes=0;
#ifdef WIN32
	ReadFile((HANDLE)mpx.fdRead,buf,bufsize-1,&bytes,NULL);
#else
	bytes=read(mpx.fdRead,buf,bufsize-1);
#endif
	*(buf+bytes)=0;
	return bytes;
}

int mpCommand(char* cmd)
{
	int ret,bytes=0;
	char* p=malloc(strlen(cmd)+2);
	ret=sprintf(p,"%s\n",cmd);
#ifdef WIN32
	WriteFile((HANDLE)mpx.fdWrite,p,ret,&bytes,NULL);
#else
	ret=write(mpx.fdWrite,p,ret);
#endif
	free(p);
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

int mpOpen(char* pchFilename)
{
	char cmd[256];

	mpClose();
#ifdef WIN32
	sprintf(cmd,"mplayer.exe -slave -quiet %s",pchFilename);
#else
	sprintf(cmd,"/cygdrive/c/mplayer/mplayer -slave -quiet %s",pchFilename);
#endif
	mpx.pchCommandLine=cmd;
	mpx.flags = SHELL_REDIRECT_STDIN;
	if (ShellExec(&mpx)) return -1;
	msleep(1000);
	mpState=1;
	return 0;
}

int ehMpd(MW_EVENT event, void* arg)
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
	char *cmd=param->pucRequest;
	if (!strncmp(cmd,"open=",5)) {
		if (mpOpen(cmd+5) || !(mpRead(param->pucBuffer,param->iDataBytes))) {
			strcpy(param->pucBuffer,"Failed to launch MPlayer");
		}
	} else if (!strncmp(cmd,"command=",8)) {
		strcpy(param->pucBuffer,(mpCommand(cmd+8)>0)?"OK":"Error");
	} else {
		return 0;
	}
	param->iDataBytes=strlen(param->pucBuffer);
	param->fileType=HTTPFILETYPE_TEXT;
	return FLAG_DATA_RAW;
}

#endif
