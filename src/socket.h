
#ifndef _UDP_PMTUD_SOCKET_H_
#define _UDP_PMTUD_SOCKET_H_

#include <netdb.h>
int create_socket(int, char *, char *, struct addrinfo *, struct addrinfo *, size_t);

#endif /* _UDP_PMTUD_SOCKET_H_ */
