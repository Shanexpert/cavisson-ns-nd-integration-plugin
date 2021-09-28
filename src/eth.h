#ifndef ETH_H 
#define ETH_H
//Call it at the onset of test
extern void init_eth_data ();
//update_eth_data updates the eth data. should be called periodically
//or just before the stats are to be taken
extern void update_eth_data(void);
//Following provided data for the last period or since init
//Duration n ms is passed from outside to provide the rate
extern u_ns_8B_t get_eth_rx_bps (int mode, unsigned int duration);
extern u_ns_8B_t get_eth_tx_bps (int mode, unsigned int duration);
extern u_ns_8B_t get_eth_rx_pps (int mode, unsigned int duration);
extern u_ns_8B_t get_eth_tx_pps (int mode, unsigned int duration);

extern u_ns_8B_t get_eth_rx_bytes();
extern u_ns_8B_t get_eth_tx_bytes();
extern u_ns_8B_t get_eth_rx_packets();
extern u_ns_8B_t get_eth_tx_packets();
#endif
