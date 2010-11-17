#ifndef KQUEUE_SERVER_H_
#define KQUEUE_SERVER_H_

#include <map>
#include <vector>

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

  class ClientWork {
  public:
    ClientTask* task;
    int number_of_bytes;
    Queue* queue;
  };

  class WorkQueue {
  public:
    void RunNextTask() {
      int result = pthread_mutex_lock(&work_mutex_);
      cout << "Mutex locked " << result << " " << endl;
      while (pending_work_.size() == 0) {
	cout << "Worker thread waiting" << endl;
	result = pthread_cond_wait(&work_cond_, &work_mutex_);
	cout << "Worker thread released: " << result << endl;
      }
      // TODO: Should get from the front of the list instead
      ClientWork* work = pending_work_.back();
      pending_work_.pop_back();
      pthread_mutex_unlock(&work_mutex_);
      work->task->Run(work->queue, work->number_of_bytes);
      cout << "Worker thread executed task" << endl;
    }

    void AddTask(ClientTask* client_task, Queue* queue, int number_of_bytes) {
      pthread_mutex_lock(&work_mutex_);
      cout << "Adding task to queue" << endl;
      ClientWork* work = new ClientWork();
      work->task = client_task;
      work->number_of_bytes = number_of_bytes;
      work->queue = queue;
      pending_work_.push_back(work);
      pthread_cond_signal(&work_cond_);
      pthread_mutex_unlock(&work_mutex_);
    }

    void Initialize() {
      int result = pthread_mutex_init(&work_mutex_, NULL);
      result = pthread_cond_init(&work_cond_, NULL);
      cout << "Work queue initialized" << endl;
    }

  private:
    pthread_mutex_t work_mutex_;
    pthread_cond_t work_cond_;
    vector<ClientWork*> pending_work_;
    vector<pthread_t*> worker_threads_;
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

}  // namespace server

#endif  // KQUEUE_SERVER_H_
