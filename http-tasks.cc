#include "http-tasks.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>

#include "logger.h"

namespace http_tasks {

  HttpTask::HttpTask(int client_fd): client_fd_(client_fd) {
    http_parser_ = new HttpParser();
  }

  void HttpTask::Initialize() {
  }

  void HttpTask::Run(Queue* queue, int available_bytes) {
    log::Debug("Client task running...");
    log::Debug("Data on client socket %d", client_fd_);
    char data[available_bytes];
    int total_read = 0;

    while (true) {
      int number_read = read(client_fd_, data, available_bytes);
      log::Debug("Read %d / %d bytes", number_read, available_bytes);
      total_read += number_read;
      if (number_read < 0 || number_read == available_bytes) {
	break;
      }
    }
    log::Debug("Data read %s", data);

    if (total_read == 0) {
      log::Error("Error: Client socket closed during reading.");
      close(client_fd_);
    } else {
      if (http_parser_->Parse(data, available_bytes)) {
	queue->Unregister(client_fd_);
	HttpResponseTask* response_task = new HttpResponseTask(client_fd_, http_parser_);
	response_task->Initialize();
	queue->Register(response_task);
      } else {
	log::Debug("Incomplete data, wait for more.");
      }
    }
  }

  // Takes ownership of the http_parser
  HttpResponseTask::HttpResponseTask(int fd, HttpParser* http_parser): client_fd_(fd),
								       http_parser_(http_parser) {
    log::Debug("HttpResponseTask created");
  }

  void HttpResponseTask::Initialize() {
  }

  void HttpResponseTask::Run(Queue* queue, int ignore) {
    log::Debug("HttpResponseTask executing");
    const HttpRequest& request = http_parser_->GetHttpRequest();

    const string status_line = "HTTP/1.1 200 OK\r\n";
    const string response = "<h1>Hello, world!</h1>"
      "<form action='http://localhost:12345' method='post'>"
      "<input type='text' name='value'/>"
      "<input type='submit' value='Submit'/>"
      "</form>";
    write(client_fd_, status_line.c_str(), status_line.length());
    write(client_fd_, "\r\n", 2);
    write(client_fd_, response.c_str(), response.length());
    log::Debug("Wrote response.");

    close(client_fd_);
    delete http_parser_;
  }

  void HttpListenTask::Initialize() {
    log::Debug("Listen task initializing...");
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 6);

    int yes = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      log::Error("setsockopt failed.");
      exit(1);
    }

    sockaddr_in server_address;
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(12345);

    int result = bind(listen_fd_, (sockaddr*) &server_address, sizeof(server_address));
    if (result != 0) {
      log::Error("Can't bind to socket");
      exit(1);
    }

    listen(listen_fd_, 5); // Max number of sockets is 5
  }

  void HttpListenTask::Run(Queue* queue) {
    log::Debug("Incoming connection");
    sockaddr_in client_address;
    socklen_t client_address_length = sizeof(sockaddr_in);
    bzero(&client_address, client_address_length);
    int incoming_fd = accept(listen_fd_, (sockaddr*) &client_address, &client_address_length);
    log::Debug("New connection accepted");
    HttpTask* client_task = new HttpTask(incoming_fd);
    client_task->Initialize();
    queue->Register(incoming_fd, client_task);
  }

  int HttpListenTask::ListenFd() {
    return listen_fd_;
  }

}  // namespace http_tasks
