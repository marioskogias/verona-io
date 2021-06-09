#include <fcntl.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <verona.h>

#define BACKLOG 8192
#define CONFIG_MAX_EVENTS 64
#define BUFSIZE 2048

using namespace verona::rt;

struct Connection : public VCown<Connection>
{
  int fd;

  Connection(int fd_) : fd(fd_) {}

  void handle()
  {
		long to_spin, reply=42;
		int ret;

		//std::cout << "Serve Behaviour" << std::endl;
		do {
			ret = recv(fd, &to_spin, sizeof(to_spin), 0);
			if (ret <= 0)
				break;
			assert(ret == sizeof(to_spin));
			std::chrono::microseconds usec(to_spin);
			auto end = std::chrono::system_clock::now() + usec;

			// spin
			while (std::chrono::system_clock::now() < end);

			ret = send(fd, &reply, sizeof(long), MSG_NOSIGNAL);
			assert(ret == sizeof(long));
		} while(1); 
  }
};

struct Poller : public VCown<Poller>
{
  int sock;
  int efd;

  static void setnonblocking(int fd)
  {
    int flags;

    flags = fcntl(fd, F_GETFL, 0);
    assert(flags >= 0);
    flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    assert(flags >= 0);
  }

  Poller(int port)
  {
    struct sockaddr_in sin;
    int one, ret, i, flags;
    struct epoll_event ev;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    assert(sock);

    setnonblocking(sock);

    one = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (void *)&one,
          sizeof(one))) {
      perror("setsockopt(SO_REUSEPORT)");
      exit(1);
    }

    one = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *)&one,
          sizeof(one))) {
      perror("setsockopt(SO_REUSEADDR)");
      exit(1);
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(0);
    sin.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *)&sin, sizeof(sin))) {
      perror("bind");
      exit(1);
    }

    if (listen(sock, BACKLOG)) {
      perror("listen");
      exit(1);
    }

    efd = epoll_create1(0);
    ev.events = EPOLLIN;
    ev.data.u32 = 0;
    ret = epoll_ctl(efd, EPOLL_CTL_ADD, sock, &ev);
    assert(!ret);
  }

  void process()
  {
    int fdcnt, i, ret, conn_sock, one = 1;
    struct epoll_event events[CONFIG_MAX_EVENTS];
    struct epoll_event ev;
    Connection *c;

    fdcnt = epoll_wait(efd, events, CONFIG_MAX_EVENTS, 0);
    for (i = 0; i < fdcnt; i++) {
      if (events[i].data.u32 == 0) {
        conn_sock = accept(sock, NULL, NULL);
        if (conn_sock == -1) {
          perror("accept");
          exit(EXIT_FAILURE);
        }
        setnonblocking(conn_sock);
        if (setsockopt(conn_sock, IPPROTO_TCP, TCP_NODELAY, (void *)&one,
              sizeof(one))) {
          perror("setsockopt(TCP_NODELAY)");
          exit(1);
        }
        // Create new Connection
        c = new Connection(conn_sock);
        ev.events = EPOLLIN | EPOLLERR;
        ev.data.ptr = c;
        if (epoll_ctl(efd, EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
          perror("epoll_ctl: EPOLL_CTL_ADD");
          exit(EXIT_FAILURE);
        }
      } else {
        c = (Connection *)events[i].data.ptr;
        if (events[i].events & (EPOLLHUP | EPOLLERR))
          /* TODO: should free conn */
          assert(0);
        else
          // Notify the connection
          schedule_lambda(c, [=](){ c->handle(); });
      }
    }
  }

#ifdef FIXED
  void notified(Object* o)
  {
    process();
  }
#elif FLOATING
  void floating_process()
  {
    process();
    schedule_lambda(this, [=](){ this->floating_process(); });
  }
#endif
};


int main(int argc, char **argv)
{

}
