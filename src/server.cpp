#include <fcntl.h>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <verona.h>

#define BACKLOG 8192

#ifdef ASIO
// ugly, but here to deal with include mess
void verona::rt::IOThread::loop()
{
	int nfds, i;
	struct epoll_event events[MAX_EVENTS];
	Cown *cown;

	nfds = epoll_wait(efd, events, MAX_EVENTS, -1);
	for (i = 0; i < nfds; i++) {
		assert(events[i].data.ptr);
		cown = static_cast<Cown *>(events[i].data.ptr);
		if (cown->io_blocked) {
			cown->io_blocked = false;
			cown->schedule();
		}
	}
}
#endif

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

		//std::cout << "Serve Behaviour" << std::endl;
		do {
			ret = recv(c->fd, &to_spin, sizeof(to_spin), 0);
			if (ret <= 0)
				break;
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
		Scheduler::register_io_fd(sock, this, EPOLLIN|EPOLLET);
	}
};

struct accept_b : public VBehaviour<accept_b> {
	struct listen_socket *s;

	accept_b(struct listen_socket *s_) : s(s_) {}

	void f()
	{
		//std::cout << "Accept Behaviour" << std::endl;
		int conn_sock, flags, one=1;

		conn_sock = accept(s->sock, NULL, NULL);
		if (conn_sock == -1) {
			assert((errno == EAGAIN) || (errno == EWOULDBLOCK));
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

		//std::cout << "Received a connection" << std::endl;

		// Create a cown, register the new conn with the efd, and schedule a serve
		auto* alloc = ThreadAlloc::get();
		struct connection *conn = new (alloc) connection(conn_sock);
		Scheduler::register_io_fd(conn_sock, conn, EPOLLIN|EPOLLERR|EPOLLET);
		Cown::schedule<serve>(conn, conn);

		// Schedule accept again
		Cown::schedule<accept_b>(s, s);
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
	//sched.set_fair(true);
	sched.init(nr_cpu);

#ifdef ASIO
	IOThread *t = new IOThread;

	sched.register_io_thread(t);
	open_server_conn(&listen_addr);
	// spawn a thread to do epoll wait
	auto thr = std::thread([=] {
			while(1) {
			t->loop();
			}
			});

	sched.run();
	thr.join();
#else
	sched.run_with_startup(&open_server_conn, &listen_addr);
#endif
	return 0;
}
