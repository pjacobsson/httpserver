#ifndef HTTP_TASKS_H_
#define HTTP_TASKS_H_

#include "http-parser.h"
#include "kqueue-server.h"

using namespace std;
using namespace server;
using namespace http_parser;

namespace http_tasks {

  // Task receiving incoming HTTP requests. Waits for the
  // entire message to be available and parsed before handing
  // over to a HttpResponseTask.
  // Highly insecure and easy to DOS.
  class HttpTask: public Task {
  public:
    HttpTask(int fd);
    void Initialize();
    void Run(Queue* queue, int available_bytes);
  private:
    int client_fd_;
    HttpParser* http_parser_;
  };

  // Task writing completed responses to the HTTP client.
  // Currently just writes a "Hello, world" message back.
  class HttpResponseTask: public Task {
  public:
    HttpResponseTask(int fd, HttpParser* http_parser);
    void Initialize();
    void Run(Queue* queue, int available_bytes);
  private:
    int client_fd_;
    HttpParser* http_parser_;
  };

  // ListenTask receiving incoming HTTP requests. Any established
  // request is delegated to a HttpTask.
  class HttpListenTask: public ListenTask {
  public:
    void Initialize();
    void Run(Queue* queue);
    int ListenFd();
  private:
    int listen_fd_;
  };

}  // namespace http_tasks

#endif  // HTTP_TASKS_H_
