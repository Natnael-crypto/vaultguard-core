#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdbool.h>

typedef enum {
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
} LogLevel;

typedef struct {
    FILE *file;
    LogLevel min_level;
    bool color;
    bool timestamp;
    char filename[256];
} Logger;

void logger_init(Logger *l, const char *filename, LogLevel min_level);
void logger_destroy(Logger *l);
void logger_log(Logger *l, LogLevel level, const char *file, int line, const char *fmt, ...);
void logger_set_level(Logger *l, LogLevel level);
const char *logger_level_name(LogLevel level);

#define LOG_TRACE(l, ...)   logger_log(l, LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(l, ...)   logger_log(l, LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(l, ...)    logger_log(l, LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(l, ...)    logger_log(l, LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(l, ...)   logger_log(l, LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(l, ...)   logger_log(l, LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)

#endif
