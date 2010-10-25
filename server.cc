#include <iostream>
#include <string>
#include <set>
#include <vector>

#include <arpa/inet.h>
#include <boost/shared_ptr.hpp>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

using namespace std;

class RequestProcessor {
public:
  void SendData(char data[], int len) {
    data[len] = 0; // Null out final for display purposes
    cout << "Received: [" << data << "] (" << len << " characters)" << endl;
  }

  bool IsComplete() {
    return true;
  }

  const char* GetResponse() {
    return kResponse;
  }

  RequestProcessor() {}

private:
  static const char* kResponse;

  RequestProcessor(const RequestProcessor&);
  RequestProcessor& operator=(const RequestProcessor&);

};

const char* RequestProcessor::kResponse = "Done!";

// TODO: Custom client that sends request slower to test this?
int respond(int client_socket, int available_bytes) {
  char data[available_bytes + 1];
  int total_read = 0;

  while (true) {
    int number_read = read(client_socket, data, available_bytes);
    cout << "Read " << number_read << " / " << available_bytes << " bytes." << endl;
    total_read += number_read;
    if (number_read == 0 || number_read == available_bytes) {
      break;
    }
  }

  RequestProcessor processor;

  if (total_read == 0) {
    cout << "Client socket closed." << endl;
    return 0;
  } else {
    processor.SendData(data, total_read);
    if (processor.IsComplete()) {
      write(client_socket, processor.GetResponse(), sizeof(processor.GetResponse()) + 1);
    }
    return total_read;
  }
}

void register_sigint(int kqueue) {
  struct kevent signal_event;
  bzero(&signal_event, sizeof(signal_event));
  EV_SET(&signal_event, SIGINT, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
  int result = kevent(kqueue, &signal_event, 1, NULL, 0, NULL);
}

class Worker {
public:
  void Initialize() {
    queue = kqueue();
    if (queue == -1) {
      cout << "client kqueue creation failed." << endl;
    }
    register_sigint(queue);
    cout << "Worker thread initialized. Queue: " << queue << " Id:" << id << endl;
  }

  void DoWork() {
    while (true) {
      struct kevent event;
      bzero(&event, sizeof(event));
      cout << "Worker thread " << id << " waiting" << endl;
      int result = kevent(queue, NULL, 0, &event, 1, NULL);
      int client_socket = event.ident;

      if (event.ident == SIGINT) {
	cout << "Shutting down worker " << id << endl;
	break;
      }

      cout << "Data on client socket " << client_socket << endl;
      int number_read = respond(client_socket, event.data);
      if (number_read == 0) {
	close(client_socket);
	struct kevent close_event;
	bzero(&close_event, sizeof(close_event));
	EV_SET(&close_event, client_socket, EVFILT_READ, EV_DELETE, 0, 0, NULL);
      }
    }
    close(queue);
    pthread_exit(0);
  }

  void Connect(int incoming_fd) {
    cout << "New client connecting (" << incoming_fd << ")" << endl;
    struct kevent client_event;
    bzero(&client_event, sizeof(client_event));
    EV_SET(&client_event, incoming_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    cout << "Adding event to queue " << queue << endl;
    int result = kevent(queue, &client_event, 1, NULL, 0, NULL);
    cout << "Client connected " << incoming_fd << endl;
  }

  explicit Worker(int id): queue(-1), id(id) {
    cout << "Starting worker " << id << endl;
  }

private:
  int queue;
  int id;

  Worker(const Worker&);
  Worker& operator=(const Worker&);
};

void* do_work_proxy(void *arg) {
  ((Worker*) arg)->DoWork();
}

// TODO: Configuration
// TODO: Logging, error handling
// TODO: Timeouts
int main() {
  cout << "Simple select server starting..." << endl;

  // TODO: Needs to support more signals
  signal(SIGINT, SIG_IGN);

  int kNumWorkers = 2;

  vector<boost::shared_ptr<Worker> > workers;
  vector<boost::shared_ptr<pthread_t> > worker_threads;
  for (int i = 0; i<kNumWorkers; ++i) {
    cout << "Creating worker " << i << endl;
    boost::shared_ptr<Worker> worker(new Worker(i));
    worker->Initialize();
    workers.push_back(worker);
    // Clean up threads when done
    boost::shared_ptr<pthread_t> worker_thread(new pthread_t());
    if (pthread_create(worker_thread.get(), NULL, &do_work_proxy, worker.get())) {
      cout << "Could not create thread\n" << endl;
      exit(1);
    }
    worker_threads.push_back(worker_thread);
  }

  // 6 = TCP. Is this constant defined anywhere?
  int listen_fd = socket(AF_INET, SOCK_STREAM, 6);

  int yes = 1;
  if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
    cout << "setsockopt failed." << endl;
    exit(1);
  }

  sockaddr_in server_address;
  bzero(&server_address, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(12345);

  int result = bind(listen_fd, (sockaddr*) &server_address, sizeof(server_address));
  if (result != 0) {
    cout << "Can't bind to socket" << endl;
    exit(1);
  }

  listen(listen_fd, 5); // Max number of sockets is 5

  int queue = kqueue();
  if (queue == -1) {
    cout << "kqueue creation failed." << endl;
  }

  struct kevent listen_event;
  bzero(&listen_event, sizeof(listen_event));
  // Why 5 ?
  EV_SET(&listen_event, listen_fd, EVFILT_READ, EV_ADD, 0, 5, NULL);
  result = kevent(queue, &listen_event, 1, NULL, 0, NULL);

  register_sigint(queue);

  int next_worker = 0;
  while (true) {
    struct kevent event;
    bzero(&event, sizeof(event));
    cout << "Main event loop waiting" << endl;
    result = kevent(queue, NULL, 0, &event, 1, NULL);
    if (event.ident == listen_fd) {
      sockaddr_in client_address;
      socklen_t client_address_length = sizeof(sockaddr_in);
      bzero(&client_address, client_address_length);
      int incoming_fd = accept(listen_fd, (sockaddr*) &client_address, &client_address_length);
      workers[next_worker]->Connect(incoming_fd);
      next_worker = (next_worker + 1) % kNumWorkers;
    } else {
      cout << "Listener received shutdown signal." << endl;
      break;
    }
  }

  close(listen_fd);
  close(queue);
  for (int i=0; i<kNumWorkers; ++i) {
    pthread_join(*worker_threads[i], NULL);
    cout << "Worker thread " << i << " closed" << endl;
  }
  return 0;
}
