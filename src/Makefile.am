bin_PROGRAMS = client server

# from logger.h
LOG_LEVEL_NOLOG	= 0xff
LOG_LEVEL_ERROR	= 0xa0
LOG_LEVEL_WARN	= 0x80
LOG_LEVEL_INFO	= 0x60
LOG_LEVEL_DEBUG	= 0x40
LOG_LEVEL_TRACE	= 0x20
LOG_LEVEL_ALL		= 0x00

AM_CFLAGS = -O1 -g -pedantic -Wall -Wextra -Werror -Wfloat-equal -Wshadow -Wpointer-arith -Wunreachable-code -Winit-self -Wno-unused-function -Wno-unused-parameter -Wno-unreachable-code
AM_CFLAGS += -DLOG_LEVEL=LOG_LEVEL_DEBUG

if !LINUX
  AM_CFLAGS += -std=c11
endif
if OSX
  AM_CFLAGS += -D__APPLE_USE_RFC_3542
endif

LDADD = -lpthread

client_SOURCES = client.c socket.c cblib.c dplpmtud.c dplpmtud_prober.c dplpmtud_ptb_handler.c dplpmtud_pl_udp.c dplpmtud_util.c
server_SOURCES = server.c socket.c cblib.c dplpmtud.c dplpmtud_prober.c dplpmtud_ptb_handler.c dplpmtud_pl_udp.c dplpmtud_util.c

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
