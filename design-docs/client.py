def process_resp(state):
    ret = read(sock.read_endpoint, buf, N)
    if ret < N:
        schedule(process_resp, (sock.read_endpoint), state)

def send_req(state):
    req = state.prepare_req()
    res = write(sock.write_end, req)
    if res < req.size():
        schedule(send_rest, (sock.write_end), state) # or req
    else:
        schedule(process_resp, (sock.read_end), state)

def main():
    sock = TCPSocket()

    state = RPCProtoInit()

    info = gethostbyname("foo.com")
    connect(sock, info) # non-bocking
    schedule(send_req, (sock.write_endpoint), state)
