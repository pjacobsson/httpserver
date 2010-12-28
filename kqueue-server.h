#ifndef KQUEUE_SERVER_H_
#define KQUEUE_SERVER_H_

#include <map>
#include <vector>

#include "util.h"
#include "http-parser.cc"

using namespace std;
using namespace util;

namespace server {

  class Queue;

  class Task {
  public:
    virtual void Initialize() = 0;
    virtual void Run(Queue* queue, int available_bytes) = 0;
  };

  class ListenTask {
  public:
    virtual void Initialize() = 0;
    virtual void Run(Queue* queue) = 0;
    virtual int ListenFd() = 0;
  };

  class Queue {
  public:
    virtual void Register(Task* task) = 0;
    virtual void Register(int fd, Task* task) = 0;
    virtual void Register(int fd, ListenTask* task) = 0;
    virtual void Unregister(int fd) = 0;
  };

  class KQueueServer: public Queue {
  public:
    virtual void Register(Task* task);
    virtual void Register(int fd, Task* task);
    virtual void Register(int fd, ListenTask* task);
    virtual void Unregister(int fd);

    void Initialize();
    void Run();
  private:
    static const char kZero;

    void ExecuteReadyTask();
    void ExecuteBlockingTask(int fd, int number_of_bytes);
    void RegisterSigint();
    void CollectGarbage();

    int queue_;
    int ready_tasks_pipe_read_;
    int ready_tasks_pipe_write_;

    SynchronizedQueue<Task> ready_tasks_;
    SynchronizedMap<int, Task> client_tasks_;
    SynchronizedMap<int, ListenTask> listen_tasks_;
    SynchronizedQueue<Task> completed_client_tasks_;
    SynchronizedQueue<ListenTask> completed_listen_tasks_;
  };

  // Acts as a load balancer between other KQueueServer
  // instances. It either processes each call to 'Register'
  // itself or delegates it to another registered server
  // in a round-robin fashion.
  class RoutingKQueueServer: public KQueueServer {
  public:
    RoutingKQueueServer();
    void Register(int fd, Task* task);
    void Register(int fd, ListenTask* task);
    void AddServer(KQueueServer* server);
  private:
    void NextServer();

    vector<KQueueServer*> servers_;
    int next_server_;
  };

}  // namespace server

#endif  // KQUEUE_SERVER_H_
