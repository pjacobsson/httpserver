#include "kqueue-server.h"

using namespace std;
using namespace server;

namespace http_tasks {

  class HttpClientTask: public ClientTask {
  public:
    HttpClientTask(int fd);
    void Initialize();
    void Run(Queue* queue, int available_bytes);
  private:
    int client_fd_;
    HttpParser http_parser_;
  };

  class HttpListenTask: public ListenTask {
  public:
    void Initialize();
    void Run(Queue* queue);
    int ListenFd();
  private:
    int listen_fd_;
  };

}  // namespace http_tasks