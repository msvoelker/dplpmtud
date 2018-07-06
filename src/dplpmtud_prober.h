
#ifndef _UDP_PMTUD_DPLPMTUD_PROBER_H_
#define _UDP_PMTUD_DPLPMTUD_PROBER_H_

void dplpmtud_ptb_received(uint32_t);
void dplpmtud_probe_acked();
void dplpmtud_start_prober(int);

uint32_t get_probe_sequence_number();

#endif /* _UDP_PMTUD_DPLPMTUD_PROBER_H_ */
