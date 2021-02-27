CC=cl
LD=link
ALLFLAGS=/O2 /W3 /Ilibctb/include /nologo /MT /DNDEBUG
CFLAGS=$(ALLFLAGS) /TC
CPPFLAGS=$(ALLFLAGS) /TP /EHsc
HTTPOBJ=httppil.obj http.obj httpxml.obj httphandler.obj httppost.obj httpauth.obj loadplugin.obj
HEADERS=httpint.h httpapi.h httpxml.h

OUTDIR=bin\\

DEFINES= /D_CRT_SECURE_NO_DEPRECATE /DNDEBUG /DNODEBUG /DWIN32
LDFLAGS= WSock32.Lib shell32.lib Iphlpapi.lib

!ifdef ENABLE_SERIAL
HTTPOBJ=$(HTTPOBJ) httpserial.obj libctb/src/fifo.obj libctb/src/serportx.obj
HEADERS=$(HEADERS) httpserial.h
DEFINES=$(DEFINES) /Ilibctb/include
!endif

default: miniweb

miniweb: miniweb.exe

min: $(HTTPOBJ) httpmin.obj
	$(LD) $(LDFLAGS) $(HTTPOBJ) httpmin.obj /OUT:$(OUTDIR)httpmin.exe

miniweb.exe: $(HTTPOBJ) miniweb.obj
	$(LD) $(LDFLAGS) $(HTTPOBJ) miniweb.obj /OUT:$(OUTDIR)miniweb.exe

all : miniweb postfile plugin

must_build:

postfile : must_build 
	$(CC) $(CFLAGS) $(DEFINES) /Ipostfile postfile/*.c /link $(LDFLAGS) /OUT:$(OUTDIR)postfile.exe

plugin : must_build
	$(CC) $(CFLAGS) /I. $(DEFINES) plugin/plugin.c /link $(LDFLAGS) /DLL /OUT:$(OUTDIR)plugin.dll

.c.obj::
	$(CC) $(DEFINES) $(CFLAGS) $<

.cpp.obj::
	$(CC) $(DEFINES) $(CPPFLAGS) $<

clean:
	del /Q *.obj
	del /Q $(OUTDIR)*.exe
	del /Q $(OUTDIR)*.obj
	del /Q $(OUTDIR)*.dll
	del /Q $(OUTDIR)*.lib

