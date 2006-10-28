CC=gcc
CFLAGS=-O2 -march=pentium2 -fomit-frame-pointer
HTTPOBJ = httppil.o http.o
HEADERS = httpint.h httpapi.h
ifndef TARGET
TARGET = miniweb
endif

ifdef POST
HTTPOBJ+= httppost.o
DEFINES+= -DHTTPPOST
endif

ifdef AUTH
HTTPOBJ+= httpauth.o
DEFINES+= -DHTTPAUTH
endif

ifndef THREAD
DEFINES+= -DNOTHREAD
endif

ifndef DEBUG
DEFINES+= -DNODEBUG
endif

ifdef MPD
DEFINES+= -D_MPD
HTTPOBJ+= mpd.o
endif

ifdef VOD
DEFINES+= -D_VOD
HTTPOBJ+= vod.o
endif

ifdef WINDIR
DEFINES= -DWIN32
LDFLAGS = -lws2_32
OS="Win32"
else
#CFLAGS+= -fPIC
ifdef THREAD
LDFLAGS = -lpthread
endif
OS="Linux"
endif

all: $(HTTPOBJ) miniweb.o
	@echo Building for $(OS)
	$(CC) $(LDFLAGS) $(HTTPOBJ) miniweb.o -o $(TARGET)

min: $(HTTPOBJ) httpmin.o
	@echo Building for $(OS)
	$(CC) $(LDFLAGS) $(HTTPOBJ) httpmin.o -o httpd

%.o: %.c $(HEADERS)
	$(CC) -c -o $@ $(CFLAGS) $(filter %.c, $^) $(DEFINES)

install: all
	@rm -f /usr/bin/$(TARGET)
	@cp $(TARGET) /usr/bin

clean:
	@rm -f $(TARGET) $(TARGET).exe
	@rm -f *.o *.ilk *.suo *.ncb
	@rm -rf Debug Release
