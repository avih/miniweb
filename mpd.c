#ifdef _MPD
#include "httppil.h"
#include "httpapi.h"
#include "procpil.h"
#include "httpxml.h"

int mpState=0;
static SHELL_PARAM mpx;

int mpRead(char* buf, int bufsize)
{
	int n;
	if (!mpx.fdStdoutRead) return 0;
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

void mpThread()
{
	char *p = NULL;
	char buf[1024];
	int n;
	int offset = 0;
	while (offset < sizeof(buf)) {
		n = mpRead(buf + offset, sizeof(buf) - offset);
		if (n <= 0) break;
		buf[offset + n] = 0;
		printf("%s", buf + offset);
		offset += n;
		if (p = strstr(buf, "Starting playback...")) break;
	}
	mpCommand("get_time_length");
	while (offset < sizeof(buf)) {
		n = mpRead(buf + offset, sizeof(buf) - offset);
		if (n <= 0) break;
		offset += n;
		if (p = strstr(buf, "ANS_LENGTH=")) break;
	}
	if (!p) mpState = 0;
}

int uhMpd(UrlHandlerParam* param)
{
	char *action;
	char *pbuf = param->pucBuffer;
	int bufsize = param->iDataBytes;
	HTTP_XML_NODE node;

	mwParseQueryString(param);
	action = mwGetVarValue(param->pxVars, "action");

	mwWriteXmlHeader(&pbuf, &bufsize, 10, "utf-8", mwGetVarValue(param->pxVars, "xsl"));
	mwWriteXmlString(&pbuf, &bufsize, 0, "<response>");

	node.indent = 1;
	node.name = "state";
	node.fmt = "%s";

	if (!strcmp(action, "open")) {
		char *filename = mwGetVarValue(param->pxVars, "stream");
		char *args = mwGetVarValue(param->pxVars, "arg");
		if (filename) mwDecodeString(filename);
		if (args) mwDecodeString(args);
		node.value = mpOpen(filename, args) ? "error" : "OK";
		mwWriteXmlLine(&pbuf, &bufsize, &node, 0);
	} else if (!strcmp(action, "command")) {
		char *cmd = mwGetVarValue(param->pxVars, "arg");
		if (cmd) {
			int bytes;
			mpCommand(cmd);
			if ((bytes = mpRead(param->pucBuffer, param->iDataBytes)) > 0) {
				node.value = "OK";
				mwWriteXmlLine(&pbuf, &bufsize, &node, 0);
				node.name = "bytes";
				node.fmt = "%d";
				node.value = (void*)bytes;
				mwWriteXmlLine(&pbuf, &bufsize, &node, 0);
				node.name = "result";
				node.fmt = "%s";
				node.value = param->pucBuffer;
				mwWriteXmlLine(&pbuf, &bufsize, &node, 0);
			} else {
				node.value = "error";
				mwWriteXmlLine(&pbuf, &bufsize, &node, 0);
			}
		}
	} else {
		return 0;
	}

	mwWriteXmlString(&pbuf, &bufsize, 0, "</response>");

	param->iDataBytes=(int)(pbuf-param->pucBuffer);
	param->fileType=HTTPFILETYPE_XML;
	return FLAG_DATA_RAW;
}

#endif
