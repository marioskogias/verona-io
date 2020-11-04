#include <fcntl.h>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <verona.h>

#define BACKLOG 8192

using namespace verona::rt;

struct serve;

struct connection : public VCown<connection> {
  int fd;
  connection(long fd_) : fd(fd_)
	{
    Cown::schedule<serve>(this, this);
	}
};

struct serve : public VBehaviour<serve> {
	struct connection *c;

	serve(struct connection *c_) : c(c_) {}

	void f()
	{
		long to_spin, reply=42;
		int ret;
		
		do {
			ret = recv(c->fd, &to_spin, sizeof(to_spin), 0);
			if (ret <= 0) {
				std::cout << "Would block" << std::endl;
				return;
			}
			assert(ret == sizeof(to_spin));
			std::chrono::microseconds usec(to_spin);
			auto end = std::chrono::system_clock::now() + usec;

			// spin
			while (std::chrono::system_clock::now() < end);

			ret = send(c->fd, &reply, sizeof(long), MSG_NOSIGNAL);
			assert(ret == sizeof(long));

		} while(1); 

		// Note that this connection would block
		c->will_block_in_io();

		// Schedule next
    Cown::schedule<serve>(c, c);
	}
};

void open_server_conn(struct sockaddr_in *sin)
{
  int sock, one, flags;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (!sock) {
    perror("socket");
    exit(1);

  }
  flags = fcntl(sock, F_GETFL, 0);
  assert(flags >= 0);
  flags = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
  assert(flags >= 0);

  one = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (void *) &one, sizeof(one))) {
    perror("setsockopt(SO_REUSEPORT)");
    exit(1);
  }

  one = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *) &one, sizeof(one))) {
    perror("setsockopt(SO_REUSEADDR)");
    exit(1);
  }

  if (bind(sock, (struct sockaddr*)sin, sizeof(*sin))) {
    perror("bind");
    exit(1);
  }

  if (listen(sock, BACKLOG)) {
    perror("listen");
    exit(1);

  }
  // Add to the initial_fds
  Scheduler::initial_fds().push_back(sock);
}

int main(int argc, char **argv)
{
  int nr_cpu;
  struct sockaddr_in listen_addr;

  std::cout << "hello!" << std::endl;
  if (argc < 3) {
    fprintf(stderr, "Usage: ./server <thread_count> <port>\n");
    return -1;
  }

  nr_cpu = atoi(argv[1]);
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_addr.s_addr = htonl(0);
  listen_addr.sin_port = htons(atoi(argv[2]));

  auto& sched = Scheduler::get();
  Scheduler::set_detect_leaks(true);
  sched.set_fair(true);
  sched.init(nr_cpu);

  sched.run_with_startup(&open_server_conn, &listen_addr);
  return 0;
}
