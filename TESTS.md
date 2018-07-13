# DPLPMTUD test scenarios

## Terms

**START, BASE, ERROR, SEARCH, DONE**
States in the state machine (see IETF draft).

**local interface MTU**
MTU of the local interface that is used to send the probes.

**PMTU**
The actual path MTU

**probe_size**
Size of the packet in bytes that is used to do the probe.

**probed_size**
Size of the packet in bytes that was used for the last successful probe.

**PTB**
ICMP Packet Too Big message. A PTB can be the ICMPv6 Packet Too Big or the ICMPv4 Destination unreachable Fragmentation needed messages.

## Tests

**Test 1**

* condition: local interface MTU = PMTU >= BASE_PMTU 
* expected behavior:
 * successful probes in START and BASE. 
 * In SEARCH, increase until probed_size = PMTU and switch to DONE.
* result: probed_size = PMTU

**Test 2**

* condition: local interface MTU = PMTU < BASE_PMTU (IPv4 only)
* expected behavior: 
 * successful probe in START 
 * probe in BASE fails -> switch to ERROR
 * successful probe in ERROR with MIN_PMTU -> switch to SEARCH 
 * In SEARCH, increase until probes_size = PMTU and switch to DONE.
* result: probed_size = PMTU 

**Test 3**

* condition:
 * local interface MTU > PMTU
 * PTBs not handled
* expected behavior:
 * successful probes in START and BASE. 
 * In SEARCH, increase until probed_size <= PMTU < probe_size 
 * recv call returns with EMSGSIZE on PTB (ignored)
 * After some retries, switch to DONE
* result: probed_size <= PMTU 

**Test 4**

* condition: 
 * local interface MTU > PMTU
 * PTBs handled but too short to validate (IPv4 only).
* expected behavior: same as Test 3
* result: same as Test 3

**Test 5**

* condition
 * local interface MTU > PMTU
 * PTBs handled long enough for validation.
* expected behavior
 * successful probes in START and BASE. 
 * In SEARCH, increase until PMTU < probe_size <= local interface MTU, which
generates a ICMP PTB message.
 * PTB sets max_pmtu to PTB_MTU. 
 * If probe with PTB_MTU succeeds -> switch to DONE 
 * Otherwise -> New PTB.
* result: probed_size = PMTU 

**Test 6**

* condition
 * local interface MTU > PMTU
 * PTBs handled but not received.
* expected behavior
 * successful probes in START and BASE. 
 * In SEARCH, increase until probed_size <= PMTU < probe_size 
 * After some retries, switch to DONE
* result: probed_size <= PMTU 

**Test 7**

* condition
 * local interface MTU > PMTU
 * PMTU changes to PMTU' while in DONE, 
 * PMTU < PMTU' < local interface MTU
* expected behavior
 * successful probes in START and BASE. 
 * In SEARCH, increase until probed_size <= PMTU < probe_size 
 * After some retries, switch to DONE
 * PMTU changes to PMTU'
 * Validation in DONE with probed_size <= PMTU < PMTU' still possible.
 * After n validations, switch to SEARCH.
 * In SEARCH increase until probed_size <= PMTU' < probe_size 
 * After some retries, switch to DONE
* result: probed_size <= PMTU'

**Test 8**

* condition
 * PMTU changes to PMTU' while in DONE
 * PMTU' < probed_size <= PMTU
* expected behavior
 * successful probes in START and BASE. 
 * In SEARCH, increase until probed_size <= PMTU < probe_size 
 * After some retries, switch to DONE
 * PMTU changes to PMTU', PMTU' < probed_size 
 * Validation in DONE fails -> switch to BASE
 * successful probe BASE -> switch to SEARCH. 
 * In SEARCH, increase until probed_size <= PMTU' < probe_size 
 * After some retries, switch to DONE
* result: probed_size <= PMTU'
