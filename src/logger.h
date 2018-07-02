#ifndef LOGGER_H
#define LOGGER_H

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

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
#define LOG_(stream, tag, message) fprintf(stream, LOG_FORMAT message "\n", time(NULL), tag, __FILE__, __LINE__);
#define LOG__(stream, tag, message, ...) fprintf(stream, LOG_FORMAT message "\n", time(NULL), tag, __FILE__, __LINE__, __VA_ARGS__);
#else
#define LOG_(stream, tag, message, ...)
#define LOG__(stream, tag, message, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOG_DEBUG(message) LOG_(stdout, "DEBUG", message)
#define LOG_DEBUG_(message, ...) LOG__(stdout, "DEBUG", message, __VA_ARGS__)
#else
#define LOG_DEBUG(message)
#define LOG_DEBUG_(message, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_INFO
#define LOG_INFO(message) LOG_(stdout, "INFO", message)
#define LOG_INFO_(message, ...) LOG__(stdout, "INFO", message, __VA_ARGS__)
#else
#define LOG_INFO(message)
#define LOG_INFO_(message, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_ERROR
#define LOG_ERROR(message) LOG_(stderr, "ERROR", message)
#define LOG_ERROR_(message, ...) LOG__(stderr, "ERROR", message, __VA_ARGS__)
#define LOG_PERROR(message) LOG__(stderr, "ERROR", "%s: " message, strerror(errno))
#define LOG_PERROR_(message, ...) LOG__(stderr, "ERROR", "%s: " message, strerror(errno), __VA_ARGS__)
#else
#define LOG_ERROR(message)
#define LOG_ERROR_(message, ...)
#define LOG_PERROR(message)
#define LOG_PERROR_(message, ...)
#endif

#endif /* LOGGER_H */
