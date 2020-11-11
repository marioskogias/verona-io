#pragma once
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pthread.h>

#define BACKLOG 8192

struct connection {
	int fd;
	pthread_spinlock_t lock;
};

int server_socket_open(struct sockaddr_in *sin);
void handle_new_conn(int server_sock, int *efds, int efd_no);
void handle_conn(struct connection *conn);
