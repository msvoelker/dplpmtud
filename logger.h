#ifndef LOGGER_H
#define LOGGER_H

#include <time.h>
#include <stdio.h>
#include <errno.h>

#define LOG_LEVEL_NOLOG	0xff
#define LOG_LEVEL_ERROR	0xc0
#define LOG_LEVEL_INFO	0x80
#define LOG_LEVEL_DEBUG	0x40
#define LOG_LEVEL_ALL		0x00

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_DEBUG
#endif

#define LOG_FORMAT "%ld %-5s %s:%d - "

#if LOG_LEVEL <= LOG_LEVEL_NOLOG
#define LOG_(stream, tag, message, args...) fprintf(stream, LOG_FORMAT message "\n", time(NULL), tag, __FILE__, __LINE__, ## args);
#else
#define LOG_(stream, tag, message, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOG_DEBUG(message, args...) LOG_(stdout, "DEBUG", message, ## args)
#else
#define LOG_DEBUG(message, args...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_INFO
#define LOG_INFO(message, args...) LOG_(stdout, "INFO", message, ## args)
#else
#define LOG_INFO(message, args...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_ERROR
#define LOG_ERROR(message, args...) LOG_(stderr, "ERROR", message, ## args)
#define LOG_PERROR(message, args...) LOG_(stderr, "ERROR", "%s: " message, strerror(errno), ## args)
#else
#define LOG_ERROR(message, args...)
#define LOG_PERROR(message, args...)
#endif

#endif /* LOGGER_H */
