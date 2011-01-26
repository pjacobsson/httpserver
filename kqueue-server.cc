#include <iostream>
#include <string>

#include <pthread.h>
#include <signal.h>
#include <sys/event.h>

#include "kqueue-server.h"
#include "logger.h"

using namespace std;

namespace server {

  const char KQueueServer::kZero = 0;

  KQueueServer::KQueueServer() {}

  void KQueueServer::Register(Task* task) {
    log::Debug("Adding client task");
    ready_tasks_.Push(task);
    write(ready_tasks_pipe_write_, &kZero, 1);
  }

  void KQueueServer::Register(int fd, Task* task) {
    log::Debug("Adding client task waiting for fd %d", fd);
    struct kevent client_event;
    bzero(&client_event, sizeof(client_event));
    EV_SET(&client_event, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    log::Debug("Adding event to kqueue %d", queue_);
    client_tasks_.Put(fd, task);
    int result = kevent(queue_, &client_event, 1, NULL, 0, NULL);
    log::Debug("Client connected %d", fd);
  }

  void KQueueServer::Register(int fd, ListenTask* task) {
    struct kevent event;
    bzero(&event, sizeof(event));
    EV_SET(&event, fd, EVFILT_READ, EV_ADD, 0, 5, NULL);
    int result = kevent(queue_, &event, 1, NULL, 0, NULL);
    log::Debug("Listen event registered %d", result);
    listen_tasks_.Put(fd, task);
  }

  void KQueueServer::Unregister(int fd) {
    ListenTask* completed_listen_task = listen_tasks_.Get(fd);
    listen_tasks_.Remove(fd);
    completed_listen_tasks_.Push(completed_listen_task);

    Task* completed_task = client_tasks_.Get(fd);
    client_tasks_.Remove(fd);
    completed_client_tasks_.Push(completed_task);

    struct kevent close_event;
    bzero(&close_event, sizeof(close_event));
    EV_SET(&close_event, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    int result = kevent(queue_, &close_event, 1, NULL, 0, NULL);
  }

  void KQueueServer::RegisterSigint() {
    struct kevent signal_event;
    bzero(&signal_event, sizeof(signal_event));
    EV_SET(&signal_event, SIGINT, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
    int result = kevent(queue_, &signal_event, 1, NULL, 0, NULL);
  }

  void KQueueServer::Initialize() {
    queue_ = kqueue();
    if (queue_ == -1) {
      log::Error("kqueue creation failed.");
    } else {
      log::Error("kqueue initialized");
    }

    client_tasks_.Initialize();
    listen_tasks_.Initialize();
    completed_client_tasks_.Initialize();
    completed_listen_tasks_.Initialize();

    RegisterSigint();

    int pipe_fd[2];
    if (pipe(pipe_fd) != 0) {
      log::Error("Failed to create pipes.");
    };
    ready_tasks_pipe_read_ = pipe_fd[0];
    ready_tasks_pipe_write_ = pipe_fd[1];
    log::Debug("Pipes initalized");

    struct kevent pipe_event;
    bzero(&pipe_event, sizeof(pipe_event));
    EV_SET(&pipe_event, ready_tasks_pipe_read_, EVFILT_READ, EV_ADD, 0, 0, NULL);
    int result = kevent(queue_, &pipe_event, 1, NULL, 0, NULL);
  }

  void KQueueServer::CollectGarbage() {
    completed_client_tasks_.Clear();
    completed_listen_tasks_.Clear();
  }

  void KQueueServer::ExecuteBlockingTask(int fd, int number_of_bytes) {
    ListenTask* listen_task = listen_tasks_.Get(fd);
    if (listen_task != NULL) {
      listen_task->Run(this);
      return;
    }

    Task* task = client_tasks_.Get(fd);
    if (task == NULL) {
      log::Debug("Error! %d was not registered", fd);
      return;
    }
    log::Debug("Number of bytes %d", number_of_bytes);
    task->Run(this, number_of_bytes);
  }

  void KQueueServer::ExecuteReadyTask() {
    char buffer[1];
    read(ready_tasks_pipe_read_, buffer, 1);
    Task* ready_task = ready_tasks_.Pop();
    if (ready_task != NULL) {
      ready_task->Run(this, -1);
    }
  }

  void KQueueServer::Run() {
    while (true) {
      CollectGarbage();

      struct kevent event;
      bzero(&event, sizeof(event));
      log::Debug("Main event loop waiting");
      int result = kevent(queue_, NULL, 0, &event, 1, NULL);

      if (event.ident == SIGINT) {
	log::Info("Shutting down worker...");
	break;
      }

      if (event.ident == ready_tasks_pipe_read_) {
	ExecuteReadyTask();
      } else {
	ExecuteBlockingTask(event.ident, event.data);
      }
    }
    close(queue_);
    close(ready_tasks_pipe_read_);
    close(ready_tasks_pipe_write_);
  }

  // ----------- RoutingKQueueServer

  RoutingKQueueServer::RoutingKQueueServer(): next_server_(-1) {
  }

  void RoutingKQueueServer::Register(int fd, Task* task) {
    NextServer();
    if (next_server_ == 0) {
      this->KQueueServer::Register(fd, task);
    } else {
      servers_[next_server_ - 1]->Register(fd, task);
    }
    log::Debug("Routed client task to server %d", next_server_);
  }

  void RoutingKQueueServer::Register(int fd, ListenTask* task) {
    NextServer();
    if (next_server_ == 0) {
      this->KQueueServer::Register(fd, task);
    } else {
      servers_[next_server_ - 1]->Register(fd, task);
    }
    log::Debug("Routed listen task to server %d", next_server_);
  }

  void RoutingKQueueServer::NextServer() {
    next_server_ = (next_server_ + 1) % (servers_.size() + 1);
    log::Debug("Next server %d", next_server_);
  }

  void RoutingKQueueServer::AddServer(KQueueServer* server) {
    servers_.push_back(server);
    log::Debug("Registered server %d", servers_.size());
  }

} // namespace server
