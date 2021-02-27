
#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <fcntl.h>  

#include "loadplugin.h"

char* mwGetVarValue(HttpVariables* vars, const char *varname, const char *defval)
{
    int i;
    if (vars && varname) {
        for (i = 0; (vars + i)->name; i++) {
            if (!strcmp((vars + i)->name, varname)) {
                return (vars + i)->value;
            }
        }
    }
    return (char*)defval;
}

PLUGIN_VISIBILITY int func1(UrlHandlerParam* hp)
{
    int ret = 0;
    int i;
    for (i = 0; i < hp->iVarCount; ++i)
        printf("%s: \"%s\"\n", hp->pxVars[i].name, hp->pxVars[i].value);
    return ret;
}

static HttpParam httpParam;

static int fdStream;

//#ifdef __GNUC__
//#   define _O_BINARY 0
//#endif
//
//PLUGIN_VISIBILITY int StreamHandlerEvent(MW_EVENT msg, int argi, void* argp)
//{
//    switch (msg) {
//    case MW_INIT:
//        fdStream = open("test.txt", O_BINARY | O_RDONLY);
//        break;
//    case MW_UNINIT:
//        _close(fdStream);
//        break;
//    }
//    return 0;
//}
//
//PLUGIN_VISIBILITY int StreamHandler(UrlHandlerParam* param)
//{
//    param->dataBytes = _read(fdStream, param->pucBuffer, param->dataBytes);
//    param->fileType = HTTPFILETYPE_TEXT;
//    return FLAG_DATA_STREAM | FLAG_CONN_CLOSE;
//}
//
struct {
    int ethif;
    char ip[16];
    int mode;
    char tvmode[4];
} cfgdata;

PLUGIN_VISIBILITY int MyUrlHandlerEvent(MW_EVENT msg, int argi, void* argp)
{
    switch (msg) {
    case MW_INIT:
        memset(&cfgdata, 0, sizeof(cfgdata));
        strcpy(cfgdata.ip, "192.168.0.100");
        cfgdata.tvmode[0] = 1;
        cfgdata.tvmode[1] = 1;
        break;
    case MW_UNINIT:
        //nothing to uninit
        break;
    }
    return 0;	//0 on success, -1 on failure
}

PLUGIN_VISIBILITY int MyUrlHandler(UrlHandlerParam* param)
{
    static const char *html_head = "<html><body><h2 align='center'>STB Configuration</h2><hr>";
    static const char *html_form_start = "<form method='get' action='/myplugin'>";
    static const char *html_ethif[] = {
        "Network Interface: <input type='radio' value='0' name='if'%s>DHCP ",
        "<input type='radio' name='if' value='1'%s>PPPoE ",
        "<input type='radio' name='if' value='2'%s>Static IP: " };
    static const char *html_ip = "<input type='text' name='ip' size='20' value='%s'></p>";
    static const char *html_mode[] = {
        "<p>Startup Mode: <select size='1' name='mode'><option value='0'%s>TV Mode</option>",
        "<option value='1'%s>VOD Mode</option>",
        "<option value='2'%s>Storage Mode</option></select></p>" };
    static const char *html_tvmode[] = {
        "<p>TV Mode Supported: <input type='checkbox' name='m0' value='1'%s>PAL ",
        "<input type='checkbox' name='m1' value='1'%s>NTSC ",
        "<input type='checkbox' name='m2' value='1'%s>720p ",
        "<input type='checkbox' name='m3' value='1'%s>1080i</p>" };
    static const char *html_form_end = "<p><input type='submit' value='Submit' name='B1'><input type='reset' value='Reset' name='B2'></p></form><hr>";
    static const char *html_tail = "</body></html>";
    static const char *tvmodes[] = { "PAL", "NTSC", "720p", "1080i" };
    char *p = param->pucBuffer, *v;
    int i;

    if (param->pxVars) {
        // processing settings
        if ((v = mwGetVarValue(param->pxVars, "if", 0)))
            cfgdata.ethif = atoi(v);
        if ((v = mwGetVarValue(param->pxVars, "mode", 0)))
            cfgdata.mode = atoi(v);
        if ((v = mwGetVarValue(param->pxVars, "ip", 0)))
            strcpy(cfgdata.ip, v);
        for (i = 0; i < 4; i++) {
            char buf[4];
            sprintf(buf, "m%d", i);
            cfgdata.tvmode[i] = mwGetVarValue(param->pxVars, buf, 0) ? 1 : 0;
        }
        // print new settings in console
        printf("\n--- Configuration ---\nNetwork Interface: %d\n", cfgdata.ethif);
        if (cfgdata.ethif == 2) printf("IP: %s\n", cfgdata.ip);
        printf("Startup Mode: %d\nTV Modes:", cfgdata.mode);
        for (i = 0; i < 4; i++) {
            if (cfgdata.tvmode[i]) printf(" %s", tvmodes[i]);
        }
        printf("\n---------------------\n\n");
    }

    p += sprintf(p, "%s%s", html_head, html_form_start);
    for (i = 0; i < 3; i++) {
        p += sprintf(p, html_ethif[i], (cfgdata.ethif == i) ? " checked" : "");
    }
    p += sprintf(p, html_ip, cfgdata.ip);
    for (i = 0; i < 3; i++) {
        p += sprintf(p, html_mode[i], (cfgdata.mode == i) ? " selected" : "");
    }
    for (i = 0; i < 4; i++) {
        p += sprintf(p, html_tvmode[i], cfgdata.tvmode[i] ? " checked" : "");
    }
    p += sprintf(p, "%s%s%s", html_form_end, param->pxVars ? "<p>New settings applied</p>" : "", html_tail);
    param->dataBytes = (int)(p - (param->pucBuffer));
    param->fileType = HTTPFILETYPE_HTML;
    return FLAG_DATA_RAW;
}

#ifdef __cplusplus
}
#endif // __cplusplus
