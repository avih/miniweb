# Miniweb plugin sytem
To add a plugin just use a C dynamic library interface.
See `plugin.c`

## How to write plugin
Compile your plugin in a separate binary.

The C interface:

    int YourUrlHandler(UrlHandlerParam* param); // PFNURLCALLBACK
    int YourUrlHandlerEvent(MW_EVENT msg, int argi, void* argp) // PFNEVENTHANDLER

The second is optional.

See `plugin`

    nmake /F Makefile.mak plugin

## Usage
From command line:

    bin\miniweb.exe -c "myplugin:bin\plugin.dll:MyUrlHandler|MyUrlHandlerEvent"

This start the default `miniweb` but loads your plugin.
You can have many `-c` arguments. See `miniweb -h`

The plugin is registered with prefix `myplugin` which means that you can access it at http://localhost/myplugin

### func1
An other simple plugin example:

    bin\miniweb.exe -c "func:bin\plugin.dll:func1"
    
Go to http://localhost/func?v=dQw4w9WgXcQ

This will parse and print the http request parameters.

    [624] request path: func1?v=ZgQMW4eVrzw
    v: "ZgQMW4eVrzw"
