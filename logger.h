#ifndef LOGGER_H_
#define LOGGER_H_

namespace log {

  void Debug(const char *fmt, ...);

  void Info(const char *fmt, ...);

  void Error(const char *fmt, ...);
}

#endif  // LOGGER_H_
