#ifndef ATH_CHANNEL_UTILITY_HH
#define ATH_CHANNEL_UTILITY_HH

#ifdef CHANNEL_UTILITY

#define CHANNEL_UTILITY_INVALID 255

#define DEFAULT_CC_PKT_UPDATE_THRESHOLD 20

#define CC_ANNO_MODE_RX_BUSY  1
#define CC_ANNO_MODE_RX_FRAME 2
#define CC_ANNO_MODE_TX_FRAME 3
#define CC_ANNO_MODE_DEFAULT  CC_ANNO_MODE_RX_BUSY
#define CC_ANNO_MODE_MAX      CC_ANNO_MODE_TX_FRAME

#define CC_UPDATE_MODE_RX                 1
#define CC_UPDATE_MODE_TXFEEDBACK         2
#define CC_UPDATE_MODE_OPERATION          4
#define CC_UPDATE_MODE_CALL               8 /* procfs */
#define CC_UPDATE_MODE_KERNELTIMER       16
#define CC_UPDATE_MODE_DEFAULT           CC_UPDATE_MODE_CALL

#define CC_UPDATE_MODE_MASK              31


struct ath_cycle_counters {
	u32 cycles;
	u32 rx_busy; /* register is called "rx clear" but it's the inverse */
	u32 rx_frame;
	u32 tx_frame;
};

struct ath_channel_utility {
	u32 busy;
	u32 rx;
	u32 tx;
};

#ifdef BRN_REGMON

#define BRN_REGMON_DEFAULT_INTERVAL   100000 /*100 ms*/
#define BRN_REGMON_DEFAULT_NO_ENTRIES 1000

struct regmon_register {
  u32 cycles;
  u32 busy_cycles;
  u32 rx_cycles;
  u32 tx_cycles;
} __attribute__((packed));

struct regmon_info {
  u16 size;
  u16 index;
} __attribute__((packed));

struct regmon_data {

  u32 jiffies;
  union {
    struct {
      u32 sec;
      u32 nsec;                      //-> sum = 12Byte
    } tv;
    u64 tv64;
  } hrtime;

  union {
    struct regmon_info     info; //4Byte
    struct regmon_register regs; //16Byte
  } value;                       //-> 16 Byte

} __attribute__((packed));                    //28Byte

#endif

void ath_hw_cycle_counters_update(struct ath_softc *sc);
uint8_t get_channel_utility(struct ath_softc *sc);

#endif

#endif
