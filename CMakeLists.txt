cmake_minimum_required (VERSION 2.6)
project (miniweb)

add_executable(miniweb 
               miniweb.c 
               httppil.c http.c httpxml.c httphandler.c httppost.c httpauth.c loadplugin.c
               win32/win_compat.h httpint.h httpapi.h httpxml.h
               )

add_subdirectory(plugin)

if(UNIX)
    add_compile_options("-pthread" "-DHAS_POSIX_TIMERS")
    target_link_libraries(miniweb dl pthread rt)
elseif(WIN32)
    target_link_libraries(miniweb Iphlpapi WSock32 Shell32)
endif(UNIX)
