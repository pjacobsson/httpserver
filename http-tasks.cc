#include "http-tasks.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>

namespace http_tasks {

  HttpClientTask::HttpClientTask(int client_fd): client_fd_(client_fd) {
  }

  void HttpClientTask::Initialize() {
  }

  void HttpClientTask::Run(Queue* queue, int available_bytes) {
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

  void HttpListenTask::Initialize() {
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

  void HttpListenTask::Run(Queue* queue) {
    cout << "Incoming connection" << endl;
    sockaddr_in client_address;
    socklen_t client_address_length = sizeof(sockaddr_in);
    bzero(&client_address, client_address_length);
    int incoming_fd = accept(listen_fd_, (sockaddr*) &client_address, &client_address_length);
    cout << "New connection accepted"<< endl;
    HttpClientTask* client_task = new HttpClientTask(incoming_fd);
    client_task->Initialize();
    queue->Register(incoming_fd, client_task);
  }

  int HttpListenTask::ListenFd() {
    return listen_fd_;
  }

} // namespace http_tasks
