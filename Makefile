CC=gcc
CFLAGS=-O2
HTTPOBJ = httppil.o http.o httpxml.o httphandler.o httppost.o
HEADERS = httpint.h httpapi.h httpxml.h
ifndef TARGET
TARGET = miniweb
endif

DEFINES=

ifdef WINDIR
DEFINES+= -DWIN32
LDFLAGS += -lwsock32
OS="Win32"
else
#CFLAGS+= -fPIC
OS="Linux"
endif

ifdef ENABLE_AUTH
HTTPOBJ+= httpauth.o
DEFINES+= -DHTTPAUTH
endif

ifndef DEBUG
DFLAGS += -s
else
DEFINES+= -D_DEBUG
LDFLAGS += -g
endif

ifdef ENABLE_MPD
DEFINES+= -D_MPD
HTTPOBJ+= mpd.o procpil.o
endif

ifdef ENABLE_VOD
DEFINES+= -D_VOD
HTTPOBJ+= httpvod.o crc32.o
endif

ifdef ENABLE_SERIAL
HTTPOBJ+= httpserial.o libctb/src/fifo.o libctb/src/serportx.o
HEADERS+= httpserial.h
DEFINES+= -Ilibctb/include
endif

%.o: %.c $(HEADERS)
	$(CC) $(DEFINES) -c -o $@ $(CFLAGS) $(filter %.c, $^)


all: $(HTTPOBJ) miniweb.o
	@echo Building for $(OS)
	$(CC) $(LDFLAGS) $(HTTPOBJ) miniweb.o -o $(TARGET)

min: $(HTTPOBJ) httpmin.o
	@echo Building for $(OS)
	$(CC) $(LDFLAGS) $(HTTPOBJ) httpmin.o -o httpd

install: all
	@rm -f /usr/bin/$(TARGET)
	@cp $(TARGET) /usr/bin

clean:
	@rm -f $(TARGET) $(TARGET).exe
	@rm -f *.o
	@rm -rf Debug Release
