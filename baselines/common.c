#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/time.h>

#include "common.h"

static void setnonblocking(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL, 0);
	assert(flags >= 0);
	flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	assert(flags >= 0);
}

static inline long time_us(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (long)tv.tv_sec * 1000000 + (long)tv.tv_usec;
}

static void spin(long usec)
{
	long start;
	start = time_us();
	while (time_us() < start + usec)
		;
}

int server_socket_open(struct sockaddr_in *sin)
{
	int sock, one;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (!sock) {
		perror("socket");
		exit(1);
	}

	setnonblocking(sock);

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

	return sock;
}

void handle_new_conn(int server_sock, int *efds, int efd_no)
{
	int i, conn_sock, one=1;
	struct epoll_event ev;
	struct connection *conn;

	conn_sock = accept(server_sock, NULL, NULL);
	if (conn_sock == -1) {
		perror("accept");
		exit(EXIT_FAILURE);
	}
	setnonblocking(conn_sock);
	if (setsockopt(conn_sock, IPPROTO_TCP, TCP_NODELAY, (void *) &one, sizeof(one))) {
		perror("setsockopt(TCP_NODELAY)");
		exit(1);
	}

	conn = malloc(sizeof(struct connection));
	assert(conn);
	conn->fd = conn_sock;
#ifdef FLOATING
	pthread_spin_init(&conn->lock, PTHREAD_PROCESS_PRIVATE);
#endif
	// Add to efd
	ev.events = EPOLLIN | EPOLLERR;
	ev.data.ptr = conn;

	for (i=0;i<efd_no;i++)
		if (epoll_ctl(efds[i], EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
			perror("epoll_ctl: EPOLL_CTL_ADD");
			exit(EXIT_FAILURE);
		}
}

void handle_conn(struct connection *conn)
{
	long to_spin, reply=42;
	int ret;
#ifdef FLOATING
	if (!pthread_spin_trylock(&conn->lock))
		return;
#endif

	do {
		ret = recv(conn->fd, &to_spin, sizeof(long), 0);
		if (ret <= 0)
			return;
		assert(ret == sizeof(long));
		spin(to_spin);

		ret = send(conn->fd, &reply, sizeof(long), MSG_NOSIGNAL);
		assert(ret == sizeof(long));

	} while(1);
#ifdef FLOATING
	pthread_spin_unlock(&conn->lock);
#endif
}
