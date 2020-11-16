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
    Each connection is 3 cowns (read_endpoint, write_endpoint, cmd_endpoint)
"""

# Called with the write_endpoint acquired
def write_rest(buf):
    res = write(write_endpoint, buf, N)
    if res < N:
        schdule(write_rest, (write_endpoint), buf)
        return
    else:
        # If used with readfn not readfn2
        schedule(readfn, (read_endpoint, write_endpoint))

# Called with read_endpoint and write_endpoint acquired
def readfn():
    while True:
        res = read(read_endpoint, buf, N)
        if res == -1:
            schedule(readfn, (read_endpoint, write_endpoint))
            break

        res = write(write_endpoint, buf, N)
        if res < N:
            schdule(write_rest, (write_endpoint), buf)
            break

# Alternative read function called with read_endpoint acquired only
def readfn2():
    while True:
        res = read(read_endpoint, buf, N)
        if res == -1:
            schedule(readfn, (read_endpoint))
            break

        schedule(write_rest, (write_endpoint), buf)


def main():
    read_endpoint, write_endpoint = TCPSocket()
    schedule(readfn, (read_endpoint, write_endpoint))

    # Or alternatively
    schedule(readfn2, (read_endpoint))
