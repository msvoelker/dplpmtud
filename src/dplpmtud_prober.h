
#ifndef _UDP_PMTUD_DPLPMTUD_PROBER_H_
#define _UDP_PMTUD_DPLPMTUD_PROBER_H_

void dplpmtud_ptb_received(uint32_t);
void dplpmtud_probe_acked();
int dplpmtud_start_prober(int);
void dplpmtud_remote_if_mtu_received(int);

uint32_t get_probe_sequence_number();

#endif /* _UDP_PMTUD_DPLPMTUD_PROBER_H_ */
