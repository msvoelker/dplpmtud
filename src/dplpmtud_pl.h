
#ifndef _DPLPMTUD_PL_H_
#define _DPLPMTUD_PL_H_

#ifdef __linux__ 
#include <bits/types.h>
#endif 

int dplpmtud_send_probe(int, uint32_t, int);
int dplpmtud_message_handler(int, void *, size_t, struct sockaddr *, socklen_t);

int verify_ptb4(char *, size_t);
int verify_ptb6(char *, size_t);

#endif /* _DPLPMTUD_UDP_H_ */
