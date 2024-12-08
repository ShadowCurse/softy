#include "defines.h"

#include <stdarg.h>
#include <stdio.h>

#define DEFAULT_COLOR "\x1b[0m"
#define WHITE "\x1b[37m"
#define HIGH_WHITE "\x1b[90m"
#define YELLOW "\x1b[33m"
#define RED "\x1b[31m"

#define INFO(...) LOG("INFO", WHITE, __VA_ARGS__)
#define WARN(...) LOG("WARN", YELLOW, __VA_ARGS__)
#define ERROR(...) LOG("ERROR", RED, __VA_ARGS__)
#define DEBUG(...) LOG("DEBUG", HIGH_WHITE, __VA_ARGS__)

#define ASSERT(statement, ...)                                                 \
  if (!statement) {                                                            \
    ERROR(__VA_ARGS__);                                                        \
    exit(1);                                                                   \
  }

#define LOG(level, color, ...)                                                 \
  _LOG(level, color, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

void _LOG(const char *level, const char *color, const char *file, u32 line,
          const char *function, const char *format, ...) {
  va_list args;

  va_start(args, format);

  char buff[1024];
  int len = snprintf(buff, sizeof(buff), "%s[%s:%s:%d:%s] %s%s", color, level,
                     file, line, function, format, DEFAULT_COLOR);
  if (len < 0) {
    printf("LOG LINE IS TOO LONG at %s:%d:%s", file, line, function);
  } else {
    vprintf(buff, args);
  }

  va_end(args);
}
