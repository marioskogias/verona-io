#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/epoll.h>

#include "common.h"

#define MAX_EVENTS 1

struct sockaddr_in listen_addr;
int nr_cpu;
#ifdef FLOATING
int *all_efds;
#endif

void *thread_main(void *arg)
{
	int i, id, sock, efd, ret, nfds, one=1;
	struct epoll_event ev, events[MAX_EVENTS];
	struct connection *conn;


	id = (int)(long)arg;
	printf("Hello from thread %d\n", id);

	sock = server_socket_open(&listen_addr);

	/* Create efd and register the new socket */ 
	efd = epoll_create1(0);
	assert(efd>0);
	ev.events = EPOLLIN;
	ev.data.u32 = 0;
	ret = epoll_ctl(efd, EPOLL_CTL_ADD, sock, &ev);
	assert(!ret);
#ifdef FLOATING
	all_efds[id] = efd;
#endif

	while(1) {
		nfds = epoll_wait(efd, events, MAX_EVENTS, -1);
		assert(nfds > 0);
		for (i = 0; i < nfds; i++) {
			if (events[i].data.u32 == 0)
#ifdef FLOATING
				handle_new_conn(sock, all_efds, nr_cpu);
#else
				handle_new_conn(sock, &efd, 1);
#endif
			else {
				conn = events[i].data.ptr;
				if (events[i].events & (EPOLLHUP | EPOLLERR)) {
					close(conn->fd);
					fprintf(stderr, "Closed conection\n");
				} else 
					handle_conn(conn);
			}
		}
	}
}

int main(int argc, char **argv)
{
	int i;
	pthread_t tid;

	if (argc < 3) {
		fprintf(stderr, "Usage: ./symmetric <thread_count> <port>\n");
		return -1;
	}

	nr_cpu = atoi(argv[1]);
	listen_addr.sin_family = AF_INET;
	listen_addr.sin_addr.s_addr = htonl(0);
	listen_addr.sin_port = htons(atoi(argv[2]));

#ifdef FLOATING
	all_efds = malloc(nr_cpu*sizeof(int));
	assert(all_efds);
#endif

	for (i = 1; i < nr_cpu; i++) {
		printf("Spawning thread %d\n", i);
		if (pthread_create(&tid, NULL, thread_main, (void *) (long) i)) {
			fprintf(stderr, "failed to spawn thread %d\n", i);
			return -1;
		}
	}

	thread_main(0);
}
