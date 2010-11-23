#include <iostream>
#include <string>

#include <pthread.h>
#include <signal.h>
#include <sys/event.h>

#include "kqueue-server.h"

using namespace std;

// TODO: Switch lock / unlock to RIAA or maybe a threadsafe collection?
namespace server {

  const char KQueueServer::kZero = 0;

  void KQueueServer::Register(Task* task) {
    cout << "Adding client task" << endl;
    pthread_mutex_lock(&ready_tasks_mutex_);
    ready_tasks_.push_back(task);
    write(ready_tasks_pipe_write_, &kZero, 1);
    pthread_mutex_unlock(&ready_tasks_mutex_);
  }

  void KQueueServer::Register(int fd, Task* task) {
    cout << "Adding client task waiting for fd" << fd << endl;
    struct kevent client_event;
    bzero(&client_event, sizeof(client_event));
    EV_SET(&client_event, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    cout << "Adding event to kqueue " << queue_ << endl;
    pthread_mutex_lock(&client_tasks_mutex_);
    client_tasks_[fd] = task;
    pthread_mutex_unlock(&client_tasks_mutex_);
    int result = kevent(queue_, &client_event, 1, NULL, 0, NULL);
    cout << "Client connected " << fd << endl;
  }

  void KQueueServer::Register(int fd, ListenTask* task) {
    struct kevent event;
    bzero(&event, sizeof(event));
    EV_SET(&event, fd, EVFILT_READ, EV_ADD, 0, 5, NULL);
    int result = kevent(queue_, &event, 1, NULL, 0, NULL);
    cout << "Listen event registered " << result << " " << endl;
    pthread_mutex_lock(&listen_tasks_mutex_);
    listen_tasks_[fd] = task;
    pthread_mutex_unlock(&listen_tasks_mutex_);
  }

  void KQueueServer::Notify(int fd, int number_of_bytes) {
    pthread_mutex_lock(&listen_tasks_mutex_);
    map<int, ListenTask*>::iterator listen_it = listen_tasks_.find(fd);
    pthread_mutex_unlock(&listen_tasks_mutex_);
    if (listen_it != listen_tasks_.end()) {
      listen_it->second->Run(this);
      return;
    }
    pthread_mutex_lock(&client_tasks_mutex_);
    map<int, Task*>::iterator client_it = client_tasks_.find(fd);
    pthread_mutex_unlock(&client_tasks_mutex_);
    if (client_it == client_tasks_.end()) {
      cout << "Error! fd " << fd << " was not registered" << endl;
      return;
    }
    cout << "Number of bytes " << number_of_bytes << endl;
    client_it->second->Run(this, number_of_bytes);
  }

  void KQueueServer::Unregister(int fd) {
    pthread_mutex_lock(&listen_tasks_mutex_);
    map<int, ListenTask*>::iterator listen_it = listen_tasks_.find(fd);
    if (listen_it != listen_tasks_.end()) {
      listen_tasks_.erase(listen_it);
      pthread_mutex_lock(&completed_listen_tasks_mutex_);
      completed_listen_tasks_.push_back(listen_it->second);
      pthread_mutex_unlock(&completed_listen_tasks_mutex_);
    }
    pthread_mutex_unlock(&listen_tasks_mutex_);

    pthread_mutex_lock(&client_tasks_mutex_);
    map<int, Task*>::iterator client_it = client_tasks_.find(fd);
    if (client_it != client_tasks_.end()) {
      client_tasks_.erase(client_it);
      pthread_mutex_lock(&completed_client_tasks_mutex_);
      completed_client_tasks_.push_back(client_it->second);
      pthread_mutex_unlock(&completed_client_tasks_mutex_);
    }
    pthread_mutex_unlock(&client_tasks_mutex_);

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
      cout << "kqueue creation failed." << endl;
    } else {
      cout << "kqueue initialized" << endl;
    }

    pthread_mutex_init(&client_tasks_mutex_, NULL);
    pthread_mutex_init(&listen_tasks_mutex_, NULL);
    pthread_mutex_init(&completed_client_tasks_mutex_, NULL);
    pthread_mutex_init(&completed_listen_tasks_mutex_, NULL);

    RegisterSigint();


    int pipe_fd[2];
    if (pipe(pipe_fd) != 0) {
      cout << "Failed to create pipes";
    };
    ready_tasks_pipe_read_ = pipe_fd[0];
    ready_tasks_pipe_write_ = pipe_fd[1];
    cout << "Pipes initalized" << endl;

    struct kevent pipe_event;
    bzero(&pipe_event, sizeof(pipe_event));
    EV_SET(&pipe_event, ready_tasks_pipe_read_, EVFILT_READ, EV_ADD, 0, 0, NULL);
    int result = kevent(queue_, &pipe_event, 1, NULL, 0, NULL);
  }

  void KQueueServer::CollectGarbage() {
    pthread_mutex_lock(&completed_client_tasks_mutex_);
    for (int i=0; i<completed_client_tasks_.size(); ++i) {
      delete completed_client_tasks_[i];
    }
    completed_client_tasks_.clear();
    pthread_mutex_unlock(&completed_client_tasks_mutex_);

    pthread_mutex_lock(&completed_listen_tasks_mutex_);
    for (int i=0; i<completed_listen_tasks_.size(); ++i) {
      delete completed_listen_tasks_[i];
    }
    completed_listen_tasks_.clear();
    pthread_mutex_unlock(&completed_listen_tasks_mutex_);
  }

  void KQueueServer::Run() {
    while (true) {
      CollectGarbage();

      struct kevent event;
      bzero(&event, sizeof(event));
      cout << "Main event loop waiting" << endl;
      int result = kevent(queue_, NULL, 0, &event, 1, NULL);

      if (event.ident == SIGINT) {
	cout << "Shutting down worker..." << endl;
	break;
      }

      if (event.ident == ready_tasks_pipe_read_) {
	pthread_mutex_lock(&ready_tasks_mutex_);
	char buffer[1];
	read(ready_tasks_pipe_read_, buffer, 1);
	Task* ready_task = NULL;
	if (ready_tasks_.size() > 0) {
	  ready_task = ready_tasks_.back();
	  ready_tasks_.pop_back();
	}
	pthread_mutex_unlock(&ready_tasks_mutex_);

	if (ready_task != NULL) {
	  ready_task->Run(this, -1);
	  continue;
	}
      }

      cout << "Incoming event" << endl;
      Notify(event.ident, event.data);
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
    cout << "Routed client task to server " << next_server_ << endl;
  }

  void RoutingKQueueServer::Register(int fd, ListenTask* task) {
    NextServer();
    if (next_server_ == 0) {
      this->KQueueServer::Register(fd, task);
    } else {
      servers_[next_server_ - 1]->Register(fd, task);
    }
    cout << "Routed listen task to server " << next_server_ << endl;
  }

  void RoutingKQueueServer::NextServer() {
    next_server_ = (next_server_ + 1) % (servers_.size() + 1);
    cout << "Next server " << next_server_ << endl;
  }

  void RoutingKQueueServer::AddServer(KQueueServer* server) {
    servers_.push_back(server);
    cout << "Registered server " << servers_.size() << endl;
  }
} // namespace server
