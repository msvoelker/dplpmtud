# Datagram Packetization Layer Path MTU Discovery (DPLPMTUD)

Implementation based on the IETF draft 

<https://tools.ietf.org/html/draft-ietf-tsvwg-datagram-plpmtud-01>

It works with IPv4 and IPv6 and uses a simple UDP based Heartbeat-Message for
probing.

## Supported OS

FreeBSD, Linux (tested with Ubuntu) and macOS (IPv6 only).

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

`src/client IP PORT`

For server, IP must be a local IP address. 
For client, IP must be the servers IP address. In order to handle ICMP PTB 
messages, client needs superuser privilege.

## Test cases 

see [TESTS.md](TESTS.md)
