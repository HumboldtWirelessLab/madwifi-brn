#ifndef ATH_CHANNEL_UTILITY_HH
#define ATH_CHANNEL_UTILITY_HH

#ifdef CHANNEL_UTILITY

#define DEFAULT_CC_PKT_UPDATE_THRESHOLD 20

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
