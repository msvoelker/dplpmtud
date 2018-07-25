# DPLPMTUD documentation

Implementation based on the IETF draft 

<https://tools.ietf.org/html/draft-ietf-tsvwg-datagram-plpmtud-01>

## Getting Started 

To start DPLPMTUD from an application, include dplpmtud.h and call this function 

```
pthread_t dplpmtud_start(int socket, int address_family, int send_probes, int handle_ptb)
```

It starts DPLPMTUD in a new thread and returns the thread id immediately. DPLPMTUD uses the passed connected socket `socket` using the address family `address_family` (AF_INET or AF_INET6). The mode of the program can be set by the parameters.
* If send_probes == 0, DPLPMTUD listens for probes and sends responses only.
* If send_probes != 0, DPLPMTUD not only listens for probes and sends responses, but also do the actual probing. 
* If send_probes != 0 and handle_ptb != 0, DPLPMTUD additionally listens for ICMP PTBs and use them in the PLPMTU process.

### Source Files 

**dplpmtud.c** main source file. In this source file, the function dplmpmtud_start starts the program.

**dplpmtud_pl_udp.c** implementation of a UDP based heartbeat message for probing. dplpmtud_pl.h specifies the interface for packetization layer implementations.

**dplpmtud_prober.c** implementation of the DPLPMTUD state machine as described in the IETF draft to discover the actual PMTU.

**dplpmtud_ptb_handler.c** checks ICMP PTB messages, verifies it, and signals the prober in case of a valid PTB.

**dplpmtud_util.c** Util functions. The dplpmtud_util_OS.c files contain OS dependent implementation of util functions.

## Details 

Upon dplpmtud_start the program starts a new thread and immediately returns the thread ID. DPLPMTUD works within this single thread.
It uses the `cblib` library as event handler. DPLPMTUD consists of three main parts.

### Listener 

Every time the passed `socket` gets readable, cblib starts the event handle function `dplpmtud_socket_readable`, which reads the message and calls the PL-dependent message handler function `dplpmtud_message_handler`. The PL-dependent functions are specified in dplpmtud_pl.h. A UDP based implementation can be found in dplpmtud_pl_udp.c.

### Prober 

If `send_probes` is enabled, it starts the actual probing (see dplpmtud_prober.c). In order to do the probing, it sets the avoid fragmentation option for `socket` (on macOS, this is not possible for IPv4; thats why DPLPMTUD supports macOS with IPv6 only). The implementation follows (with some exceptions) the state machine from the IETF draft. For each state there is a `STATE_run` function, which starts the state *STATE*. After calling the run function, a state usually waits for events. For each of the three common events (probe acked, probe failed, and ptb received) there is a corresponding function (`STATE_probe_acked`, `STATE_probe_failed`, and `STATE_ptb_received`). Not every state implements all three functions.

If a probe ack is received, 
* cblib calls `dplpmtud_socket_readable()` in dplpmtud.c.
* `dplpmtud_socket_readable()` calls the the PL-dependent function `dplpmtud_message_handler()` specified by dplpmtud_pl.h.
* PL implementation (see dplpmtud_pl_udp.c) should be able to match the probe ack to the request. In case of a positive match, it calls `dplpmtud_probe_acked()` in dplpmtud_prober.c.
* `dplpmtud_probe_acked()` calls the `STATE_probe_acked()` function.

If a probe failed (probe timer expired),
* cblib calls `on_probe_timer_expired()` in dplpmtud_prober.c.
* `on_probe_timer_expired()` realises that probe timer expired for the *MAX_PROBES* time and calls `STATE_probe_failed()`.

If a valid PTB message is received,
* cblib calls `dplpmtud_icmp_socket_readable()` in dplpmtud_ptb_handler.c.
* `dplpmtud_icmp_socket_readable()` reads the message and calls `ptb4_handler()` or `ptb6_handler()`.
* `ptb4/6_handler()` let the PL-dependent function `verify_ptb4()` or `verify_ptb6()` (specified by plpmtud_pl.h) verify the PTB.
* On a valid PTB, `ptb4/6_handler()` calls `dplpmtud_ptb_received()` in dplpmtud_prober.c.
* `dplpmtud_probe_acked()` calls the `STATE_ptb_received()` function if either PTB limit is disabled (ptb_mtu_limit == 0) or the reported MTU is less than ptb_mtu_limit.

In START-state, probe_size is successively increased until `current_max_pmtu`. The value `current_max_pmtu` is the minimum of local interface MTU, remote peers interface MTU, and the constant `MAX_PMTU` (see `update_max_pmtu()` function). To determine the remote peers interface MTU, in the last probe before the START-state (in BASE, ERROR, and DONE state), a flag is set that tells the peer to add its local interface MTU in the probe reply.

### PTB Handler 

If `send_probes` and `handle_ptb` is enabled, it creates a raw ICMP socket (see `dplpmtud_ptb_handler_init()` in dplpmtud_ptb_handler.c). Every time this socket gets readable, cblib start the event handle function `dplpmtud_icmp_socket_readable()` in dplpmtud_ptb_handler.c. It let the PL-dependent function `verify_ptb4()` or `verify_ptb6()`verify the PTB and signals the event to the prober (see section Prober).
