#include "logger.h"

#include <stdio.h>
#include <stdarg.h>

namespace log {

  void Debug(const char *fmt, ...) {
    fprintf(stderr, "%s", "DEBUG -- ");
    va_list argp;
    va_start(argp, fmt);
    vfprintf(stderr, fmt, argp);
    va_end(argp);
    fprintf(stderr, "\n");
  };

  void Info(const char *fmt, ...) {
    fprintf(stderr, "%s", "INFO -- ");
    va_list argp;
    va_start(argp, fmt);
    vfprintf(stderr, fmt, argp);
    va_end(argp);
    fprintf(stderr, "\n");
  };

  void Error(const char *fmt, ...) {
    fprintf(stderr, "%s", "ERROR -- ");
    va_list argp;
    va_start(argp, fmt);
    vfprintf(stderr, fmt, argp);
    va_end(argp);
    fprintf(stderr, "\n");
  };
}
