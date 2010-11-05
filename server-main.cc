
#include "kqueue-server.h"

using namespace server;

int main() {
  cout << "Simple server starting..." << endl;
  // TODO: Needs to support more signals
  signal(SIGINT, SIG_IGN);

  KQueueServer server;
  ListenTask listen_task;
  listen_task.Initialize();
  server.Initialize();
  server.Register(listen_task.ListenFd(), &listen_task);
  server.Run();
}
