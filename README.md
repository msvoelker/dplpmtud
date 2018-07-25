# Datagram Packetization Layer Path MTU Discovery (DPLPMTUD)

Implementation based on the IETF draft 

<https://tools.ietf.org/html/draft-ietf-tsvwg-datagram-plpmtud-01>

It works with IPv4 and IPv6 and uses a simple UDP based Heartbeat-Message for
probing.

## Supported OS

* FreeBSD
* Linux (tested with Ubuntu)
* macOS (IPv6 only).

## Build

```
autoreconf --install
./configure
make
```

## Run 

Start the server on one host with 

`src/server IP PORT`

and the client on the other host with 

`src/client [--handle-ptb] IP PORT`

For the server, IP must be a local IP address and PORT a local free port the server can listen on. 

For the client, choose the same IP and PORT as chosen for the server. If the client shall handle ICMP PTB messages, add the option --handle-ptb. For this, the client needs superuser privilege.

## See also 

Test scenarios [TESTS.md](TESTS.md).

Documentation [DOC.md](DOC.md).
