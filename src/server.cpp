#include <fcntl.h>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

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

struct listen_socket : public VCown<listen_socket> {
  int sock;

  listen_socket(struct sockaddr_in *sin)
  {
    int one, flags;

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
    Scheduler::register_io_fd(sock, this, EPOLLIN);
  }
};

struct accept_b : public VBehaviour<accept_b> {
	struct listen_socket *s;

	accept_b(struct listen_socket *s_) : s(s_) {}

  void f()
  {
    int conn_sock, flags, one=1;

    conn_sock = accept(s->sock, NULL, NULL);
    if (conn_sock == -1) {
      assert((errno == EAGAIN) || (errno == EWOULDBLOCK));
      std::cout << "Tried to accept" << std::endl;
      s->will_block_in_io();
      Cown::schedule<accept_b>(s, s);
      return;
    }
    flags = fcntl(conn_sock, F_GETFL, 0);
    assert(flags >= 0);
    flags = fcntl(conn_sock, F_SETFL, flags | O_NONBLOCK);
    assert(flags >= 0);

    if (setsockopt(conn_sock, IPPROTO_TCP, TCP_NODELAY, (void *) &one, sizeof(one))) {
      perror("setsockopt(TCP_NODELAY)");
      exit(1);
    }

    std::cout << "Received a connection" << std::endl;
    assert(0);
#if 0
    conn = malloc(sizeof *conn);
#if CONFIG_REGISTER_FD_TO_ALL_EPOLLS
    conn->lock = 0;
#endif
    conn->fd = conn_sock;
    conn->state = STATE_HEADER;
    conn->buf_head = 0;
    conn->buf_tail = 0;
    epoll_ctl_add(conn_sock, conn);
#endif
  }
};

void open_server_conn(struct sockaddr_in *sin)
{
  auto* alloc = ThreadAlloc::get();
  struct listen_socket *server_sock = new (alloc) listen_socket(sin);

  // Schedule an accept_b behaviour
  Cown::schedule<accept_b>(server_sock, server_sock);
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
