#include <iostream>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include "kqueue-server.h"

using namespace std;

// TODO: Switch lock / unlock to RIAA or maybe a threadsafe collection?
namespace server {

  void KQueueServer::Register(int fd, ClientTask* task) {
    cout << "Adding client task" << endl;
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
    map<int, ClientTask*>::iterator client_it = client_tasks_.find(fd);
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
    map<int, ClientTask*>::iterator client_it = client_tasks_.find(fd);
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

      cout << "Incoming event" << endl;
      Notify(event.ident, event.data);
    }
    close(queue_);
  }

  // ----------- RoutingKQueueServer

  RoutingKQueueServer::RoutingKQueueServer(): next_server_(-1) {
  }

  void RoutingKQueueServer::Register(int fd, ClientTask* task) {
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

  // ----------- Listen task

  void ListenTask::Initialize() {
    cout << "Listen task initializing..." << endl;
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 6);

    int yes = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      cout << "setsockopt failed." << endl;
      exit(1);
    }

    sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(12345);

    int result = bind(listen_fd_, (sockaddr*) &server_address, sizeof(server_address));
    if (result != 0) {
      cout << "Can't bind to socket" << endl;
      exit(1);
    }

    listen(listen_fd_, 5); // Max number of sockets is 5
  }

  void ListenTask::Run(Queue* queue) {
    cout << "Incoming connection" << endl;
    sockaddr_in client_address;
    socklen_t client_address_length = sizeof(sockaddr_in);
    bzero(&client_address, client_address_length);
    int incoming_fd = accept(listen_fd_, (sockaddr*) &client_address, &client_address_length);
    cout << "New connection accepted"<< endl;
    ClientTask* client_task = new ClientTask(incoming_fd);
    client_task->Initialize();
    queue->Register(incoming_fd, client_task);
  }

  int ListenTask::ListenFd() {
    return listen_fd_;
  }

  // ----------- Client task

  ClientTask::ClientTask(int client_fd): client_fd_(client_fd) {
  }

  void ClientTask::Initialize() {
  }

  void ClientTask::Run(Queue* queue, int available_bytes) {
    cout << "Client task running..." << endl;
    cout << "Data on client socket " << client_fd_ << endl;
    char data[available_bytes];
    int total_read = 0;

    while (true) {
      int number_read = read(client_fd_, data, available_bytes);
      cout << "Read " << number_read << " / " << available_bytes << " bytes." << endl;
      total_read += number_read;
      if (number_read < 0 || number_read == available_bytes) {
	break;
      }
    }

    cout << "Data read " << data << endl;

    if (total_read == 0) {
      cout << "Error: Client socket closed during reading." << endl;
      close(client_fd_);
    } else {
      if (http_parser_.Parse(data, available_bytes)) {
	const HttpRequest& request = http_parser_.GetHttpRequest();

	const string status_line = "HTTP/1.1 200 OK\r\n";
	const string response = "<h1>Hello, world!</h1>";
	write(client_fd_, status_line.c_str(), status_line.length());
	write(client_fd_, "\r\n", 2);
	write(client_fd_, response.c_str(), response.length());

	cout << "Wrote response." << endl;

	close(client_fd_);
	queue->Unregister(client_fd_);
      } else {
	cout << "Incomplete data, wait for more." << endl;
      }
    }
  }

} // namespace server
