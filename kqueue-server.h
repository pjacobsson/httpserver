#ifndef KQUEUE_SERVER_H_
#define KQUEUE_SERVER_H_

#include <map>
#include <vector>

#include "util.h"

using namespace std;
using namespace util;

namespace server {

  class Queue;

  // Interface defining a task, the main unit of execution.
  class Task {
  public:
    virtual void Initialize() = 0;

    // Executes this task.
    virtual void Run(Queue* queue, int available_bytes) = 0;
  };

  // Interface defining a listening task, the entry point for clients
  // connecting to the server. The typical case being a task listening
  // for new connection to a socket.
  class ListenTask {
  public:
    virtual void Initialize() = 0;

    // Executes this task when there's an incoming request on
    // the file descriptor. Schedule next tasks on the supplied
    // queue.
    virtual void Run(Queue* queue) = 0;

    // The file descriptor to listen to.
    virtual int ListenFd() = 0;
  };

  // Main interface for the queueing / scheduling mechanism. Uses
  // the typical inversion-of-control pattern.
  // All methods take ownership of the tasks that get passed in.
  class Queue {
  public:
    // Register a task to be executed asynchronously
    virtual void Register(Task* task) = 0;

    // Register a task that will be executed asynchronously
    // when there is data available on the specified file
    // descriptor.
    virtual void Register(int fd, Task* task) = 0;

    // Register a listen task, waiting incoming connections on
    // the specified file descriptor.
    virtual void Register(int fd, ListenTask* task) = 0;

    // Stop listening on this file descriptor, and delete any
    // task associated with it.
    virtual void Unregister(int fd) = 0;
  };


  // Queue implementation that uses kqueue as an underlying
  // implementation.
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

    KQueueServer(const KQueueServer&);
    KQueueServer& operator=(const KQueueServer&);
  };

  // Acts as a load balancer between other KQueueServer
  // instances. It either processes each call to 'Register'
  // itself or delegates it to another registered server
  // in a round-robin fashion. This allows multiple threads / cores
  // to be used to serve requests.
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

    RoutingKQueueServer(const RoutingKQueueServer&);
    RoutingKQueueServer& operator=(const RoutingKQueueServer&);
  };

}  // namespace server

#endif  // KQUEUE_SERVER_H_
