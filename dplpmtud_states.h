
#ifndef _UDP_PMTUD_DPLPMTUD_STATES_H_
#define _UDP_PMTUD_DPLPMTUD_STATES_H_

typedef enum { 
	START=0,
	DISABLED,
	BASE,
	SEARCH,
	ERROR,
	DONE 
} state_t;
//typedef struct instance_data instance_data_t;
typedef state_t state_func_t(); //instance_data_t *data );

state_t run_base_state();
state_t run_search_state();
state_t run_error_state();
state_t run_done_state();

state_func_t* const state_table[] = {
	NULL, 
	NULL, 
	run_base_state, 
	run_search_state, 
	run_error_state, 
	run_done_state
};

state_t run_state(state_t state) {
	return state_table[state]();
}

#endif /* _UDP_PMTUD_DPLPMTUD_STATES_H_ */
