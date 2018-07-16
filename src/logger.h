#ifndef LOGGER_H
#define LOGGER_H

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define LOG_LEVEL_NOLOG	0xff
#define LOG_LEVEL_ERROR	0xa0
#define LOG_LEVEL_WARN	0x80
#define LOG_LEVEL_INFO	0x60
#define LOG_LEVEL_DEBUG	0x40
#define LOG_LEVEL_TRACE	0x20
#define LOG_LEVEL_ALL		0x00

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#define LOG_FORMAT "%ld %-5s %s:%d - "

#if LOG_LEVEL <= LOG_LEVEL_NOLOG
#define LOG_(tag, message) fprintf(stderr, LOG_FORMAT message "\n", time(NULL), tag, __FILE__, __LINE__);
#define LOG__(tag, message, ...) fprintf(stderr, LOG_FORMAT message "\n", time(NULL), tag, __FILE__, __LINE__, __VA_ARGS__);
#else
#define LOG_(tag, message, ...)
#define LOG__(tag, message, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_TRACE
#define LOG_TRACE_ENTER LOG__("TRACE", "enter %s", __func__)
#define LOG_TRACE_LEAVE LOG__("TRACE", "leave %s", __func__)
#else
#define LOG_TRACE_ENTER
#define LOG_TRACE_LEAVE
#endif

#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define LOG_DEBUG(message) LOG_("DEBUG", message)
#define LOG_DEBUG_(message, ...) LOG__("DEBUG", message, __VA_ARGS__)
#else
#define LOG_DEBUG(message)
#define LOG_DEBUG_(message, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_INFO
#define LOG_INFO(message) LOG_("INFO", message)
#define LOG_INFO_(message, ...) LOG__("INFO", message, __VA_ARGS__)
#else
#define LOG_INFO(message)
#define LOG_INFO_(message, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_WARN
#define LOG_WARN(message) LOG_("WARN", message)
#define LOG_WARN_(message, ...) LOG__("WARN", message, __VA_ARGS__)
#else
#define LOG_WARN(message)
#define LOG_WARN_(message, ...)
#endif

#if LOG_LEVEL <= LOG_LEVEL_ERROR
#define LOG_ERROR(message) LOG_("ERROR", message)
#define LOG_ERROR_(message, ...) LOG__("ERROR", message, __VA_ARGS__)
#define LOG_PERROR(message) LOG__("ERROR", "%s: " message, strerror(errno))
#define LOG_PERROR_(message, ...) LOG__("ERROR", "%s: " message, strerror(errno), __VA_ARGS__)
#else
#define LOG_ERROR(message)
#define LOG_ERROR_(message, ...)
#define LOG_PERROR(message)
#define LOG_PERROR_(message, ...)
#endif

#endif /* LOGGER_H */
