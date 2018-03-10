#include "loadplugin.h"

#ifdef _MSC_VER
#   include <windows.h>
#else
#   include <dlfcn.h>
#endif // _MSC_VER

void* LoadDll(const char* arg)
{
#ifdef _MSC_VER
    return LoadLibraryA(arg);
#else
    return dlopen(arg, RTLD_LAZY);
#endif // _MSC_VER
}

int FreeDll(void* handle)
{
#ifdef _MSC_VER
    return FreeLibrary((HANDLE)handle);
#else
    return dlclose(handle);
#endif // _MSC_VER
}

const char* GetError()
{
#ifdef _MSC_VER
    static char error[10];
    sprintf_s(error, 10, "%d", GetLastError());
    return error;
#else
    return dlerror();
#endif // _MSC_VER
}

#ifdef _MSC_VER
#   define GetSymbol GetProcAddress
#else
#   define GetSymbol dlsym
#endif // _MSC_VER

int get_handler_list_length(UrlHandler* urlHandlerList)
{
    int n = 0;
    if (urlHandlerList)
        for (n = 0; urlHandlerList[n].pchUrlPrefix; n++)
            ;
    return n;
}

void add_handler(UrlHandler** urlHandlerList, const char* prefix, PFNURLCALLBACK uhf, PFNEVENTHANDLER ehf)
{
    int n, length;
    length = get_handler_list_length(*urlHandlerList);
    UrlHandler* new_list = (UrlHandler*)calloc(length + 2, sizeof(UrlHandler));
    if (new_list)
    {
        for (n = 0; n < length; ++n)
        {
            new_list[n] = (*urlHandlerList)[n];
        }
        new_list[n].pchUrlPrefix = prefix;
        new_list[n].pfnUrlHandler = uhf;
        new_list[n].pfnEventHandler = ehf;
        new_list[n].p_sys = NULL;

        new_list[n + 1].pchUrlPrefix = NULL;

        free(*urlHandlerList);
        *urlHandlerList = new_list;
    }
}

void add_handler_from_dll(UrlHandler** urlHandlerList, const char* arg)
{
    char* name, *function_name, *prefix, *tmp, *event_handler_name;
    void* library;
    PFNEVENTHANDLER ehf = NULL;
    PFNURLCALLBACK uhf;

    name = malloc(strlen(arg) + 1);
    strcpy(name, arg);
    function_name = strrchr(name, ':');
    prefix = strchr(name, ':');

    if (prefix != NULL && function_name > prefix + 1)
    {
        *function_name = '\0';
        ++function_name;
        *prefix = '\0';
        if (*function_name)
        {
            ++prefix;
            tmp = prefix;
            prefix = name;
            name = tmp;
            fprintf(stderr, "Loading handler... prefix: %s, dll: %s, function: %s\n", prefix, name, function_name);
            library = LoadDll(name);
            if (library)
            {
                event_handler_name = strrchr(function_name, '|');
                if (event_handler_name)
                    *event_handler_name++ = '\0';
                ehf = (PFNEVENTHANDLER)GetSymbol(library, event_handler_name);
                uhf = (PFNURLCALLBACK)GetSymbol(library, function_name);
                if (uhf)
                    add_handler(urlHandlerList, prefix, uhf, ehf);
                else
                {
                    fprintf(stderr, "couldn't load %s (Error: %s)!\n", function_name, GetError());
                    FreeDll(library);
                }
            }
            else
                fprintf(stderr, "couldn't load \"%s\" (Error: %s)!\n", name, GetError());
        }
        else
            fprintf(stderr, "function name shouldn't be empty in \"%s\"!\n", arg);
    }
    else
        fprintf(stderr, "Two colons should be in \"%s\"!\n", arg);
}
