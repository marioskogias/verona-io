"""
Clarification Questions:
    - What are the guarantees that cowns provide?
    - Do we want to provide the same guarantees to sockets?
    - What happens if a behaviour that needs to read and write
      cannot read? Should it also block other behaviours that only want to write?
    - If I understand correctly in the pony model there's only one reading function
      and reading never happens actively. It's always a callback the returns the
      data already read. Is this limiting?

Opinions:
    - We should keep and use the cown abstraction instead of introducing another one.
    - Read/Write operations have to be non blocking. We should expose their failures.

Suggestion:
    Each connection is 3 cowns (read_fd, write_fd, cmd_fd)
"""

# Called with the write_fd acquired
def write_rest(buf):
    res = write(write_fd, buf, N)
    if res < N:
        schdule_when_write_available(write_rest, buf)
        return
    else:
        # If used with readfn not readfn2
        schedule(readfn, (read_fd, write_fd))

# Called with read_fd and write_fd acquired
def readfn():
    while True:
        res = read(read_fd, buf, N)
        if res == -1:
            schedule_when_read_available(readfn, (read_fd, write_fd))
            break

        res = write(write_fd, buf, N)
        if res < N:
            schdule_when_write_available(write_rest, (write_fd), buf)

# Alternative read function called with read_fd acquired only
def readfn2():
    while True:
        res = read(read_fd, buf, N)
        if res == -1:
            schedule_when_read_available(readfn)
            break

        schedule(write_rest, buf)


def main():
    read_fd, write_fd = TCPSocket()
    schedule(readfn, (read_fd, write_fd))

    # Or alternatively
    schedule(readfn2, (read_fd))
