#ifndef KQUEUE_SERVER_H_
#define KQUEUE_SERVER_H_

#include <map>

#include "http-parser.cc"

using namespace std;

namespace server {

  class ClientTask;
  class ListenTask;

  class Queue {
  public:
    virtual void Register(int fd, ClientTask* task) = 0;
    virtual void Register(int fd, ListenTask* task) = 0;
    virtual void Unregister(int fd) = 0;
    virtual void Notify(int fd, int number_of_bytes) = 0;
  };

  class KQueueServer: public Queue {
  public:
    void Initialize();
    void Run();
    void Register(int fd, ClientTask* task);
    void Register(int fd, ListenTask* task);
    void Unregister(int fd);
    void Notify(int fd, int number_of_bytes);
  private:
    void RegisterSigint();

    int queue_;
    map<int, ClientTask*> client_tasks_;
    map<int, ListenTask*> listen_tasks_;
  };

  // TODO: Better name?
  class ClientTask {
  public:
    ClientTask(int fd);
    void Initialize();
    void Run(Queue* queue, int available_bytes);
  private:
    int client_fd_;
    HttpParser http_parser_;
  };

  class ListenTask {
  public:
    void Initialize();
    void Run(Queue* queue);
    int ListenFd();
  private:
    int listen_fd_;
  };

}  // namespace server

#endif  // KQUEUE_SERVER_H_
