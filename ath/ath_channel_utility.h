#ifndef ATH_CHANNEL_UTILITY_HH
#define ATH_CHANNEL_UTILITY_HH

#ifdef CHANNEL_UTILITY

#define DEFAULT_CC_PKT_UPDATE_THRESHOLD 20

#define CC_MODE_RX_BUSY	1
#define CC_MODE_RX_FRAME	2
#define CC_MODE_TX_FRAME	3
#define CC_MODE_DEFAULT 	CC_MODE_RX_BUSY
#define CC_MODE_MAX	 	CC_MODE_TX_FRAME

struct ath_cycle_counters {
	u32 cycles;
	u32 rx_busy; /* register is called "rx clear" but it's the inverse */
	u32 rx_frame;
	u32 tx_frame;
};

void ath_hw_cycle_counters_update(struct ath_softc *sc);
uint8_t get_channel_utility(struct ath_softc *sc);

#endif

#endif
