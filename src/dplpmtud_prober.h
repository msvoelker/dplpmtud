
#ifndef _UDP_PMTUD_DPLPMTUD_PROBER_H_
#define _UDP_PMTUD_DPLPMTUD_PROBER_H_

void *dplpmtud_prober(void *);
int signal_probe_return();
int signal_probe_return_with_mtu(uint32_t);
void dplpmtud_prober_init();

uint32_t get_probe_sequence_number();

#endif /* _UDP_PMTUD_DPLPMTUD_PROBER_H_ */
