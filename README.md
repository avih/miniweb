# miniweb-avih
Small, cross-platform HTTP server

*This project is not maintained, and should not be considered secure.*

- This is a fork of http://miniweb.sourceforge.net/ , with the following enhancements:
  - Enhanced directory listing with sortable columns (requires js, else the normal listing).
  - Unicode support on windows - command line and directory listing.
  - Better support for large files in download (correct size at the HTTP header) and directory listing
  (displayed size is now correct).
  - Linux/OS X/etc: faster shutdown, and less wait before next successful startup.
  - POST support disabled for now.
  - Easier build  with custon `$CC`, and other minor improvements. The MSVC project files might be stale.
  - Name changed to `Miniweb-avih`.


- License: "GNU Library or Lesser General Public License version 2.0 (LGPLv2)".
See [License note](./miniweb-avih/LICENSE.md).
- A snapshot of the original repository 
[svn rev 208](https://sourceforge.net/p/miniweb/code/208/) on which this fork is
based is at branch `svn-orig-rev-208` of this repository (the original project seems
unmaintained since 2013.).


## Building on Windows with msys2

```shell
pacman -S mingw-w64-x86_64-toolchain upx    # And press enter to install all
msys2 -mingw64                              # Start msys2 with path to use mingw64
make
upx miniweb.exe                             # Optional, compress the exe file
```

## Adding a new MIME type

In httpint.h:55 section, add your new file extension:
```c
#define FILEEXT_SVG DEFDWORD('S', 'V', 'G', 0)
```

In http.c:2269 section, add the mapping between the new file extension and the offset of the filetype:
```c
		case FILEEXT_SVG:	return HTTPFILETYPE_SVG;
```

In httpapi.h:56 section add the new offset at the end of enum:
```c
  HTTPFILETYPE_SVG,
```

In http.c:76 section add your new MIME type at the end of the table, to match exactly the same position as the entry in the above enum:
```c
	"image/svg+xml",
```


### [Original project description below]

## MiniWeb
MiniWeb is an embeddable, cross-platform, small-footprint HTTP server
implementation, implementing basic GET and POST requests as well as request
handling dynamic content generating. It works on x86 (Windows/Linux),
ARM, MIPS and any posix platforms, either embedded or standalone.

## Features
- small footprint HTTP server written in C
- GET & POST actions with basic HTTP authentication
- user-defined request handler routines
- cross-platform compatibility
- basic HTTP audio and video streaming
- serial UART to HTTP gateway
