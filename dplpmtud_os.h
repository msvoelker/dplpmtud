
#ifndef _DPLPMTUD_OS_H_
#define _DPLPMTUD_OS_H_

int get_local_if_mtu(int);
int get_src_addr(int, struct sockaddr_storage *, socklen_t *);

#endif /* _DPLPMTUD_OS_H_ */
