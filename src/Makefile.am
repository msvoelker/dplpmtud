bin_PROGRAMS = client server

AM_CFLAGS = -O1 -g -pedantic -Wall -Wextra -Werror -Wfloat-equal -Wshadow -Wpointer-arith -Wunreachable-code -Winit-self -Wno-unused-function -Wno-unused-parameter -Wno-unreachable-code

if !LINUX
  AM_CFLAGS += -std=c11
endif
if OSX
  AM_CFLAGS += -D__APPLE_USE_RFC_3542
endif

LDADD = -lpthread

client_SOURCES = client.c socket.c dplpmtud.c dplpmtud_listener.c dplpmtud_prober.c dplpmtud_ptb_listener.c dplpmtud_pl_udp.c dplpmtud_util.c
server_SOURCES = server.c socket.c dplpmtud.c dplpmtud_listener.c dplpmtud_prober.c dplpmtud_ptb_listener.c dplpmtud_pl_udp.c dplpmtud_util.c

if LINUX 
  client_SOURCES += dplpmtud_util_linux.c
  server_SOURCES += dplpmtud_util_linux.c
endif
if FREEBSD
  client_SOURCES += dplpmtud_util_freebsd.c
  server_SOURCES += dplpmtud_util_freebsd.c
endif
if OSX
  client_SOURCES += dplpmtud_util_osx.c
  server_SOURCES += dplpmtud_util_osx.c
endif
