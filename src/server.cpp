#include <iostream>
#include <sys/socket.h>

#include <verona.h>

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


int main()
{
  std::cout << "hello!" << std::endl;

  auto& sched = Scheduler::get();

  Scheduler::set_detect_leaks(true);
  sched.set_fair(true);
  sched.init(4);

  sched.run();
  return 0;
}
