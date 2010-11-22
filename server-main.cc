
#include "kqueue-server.h"
#include "http-tasks.h"

#include <pthread.h>

using namespace server;
using namespace http_tasks;

void* do_work_proxy(void *arg) {
  ((KQueueServer*) arg)->Run();
}

int main() {
  cout << "Simple server starting..." << endl;
  // TODO: Needs to support all relevant signals
  signal(SIGINT, SIG_IGN);

  RoutingKQueueServer router;
  KQueueServer server;
  HttpListenTask listen_task;
  listen_task.Initialize();
  router.Initialize();
  server.Initialize();
  router.AddServer(&server);
  router.Register(listen_task.ListenFd(), &listen_task);

  pthread_t worker_thread;
  pthread_create(&worker_thread, NULL, &do_work_proxy, &server);

  router.Run();
}
