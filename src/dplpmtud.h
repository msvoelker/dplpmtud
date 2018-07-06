
#ifndef _UDP_PMTUD_DPLPMTUD_H_
#define _UDP_PMTUD_DPLPMTUD_H_

#include <pthread.h>

pthread_t dplpmtud_start(int, int, int, int);
void dplpmtud_wait();

#endif /* _UDP_PMTUD_DPLPMTUD_H_ */
