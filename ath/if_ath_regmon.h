#ifndef _DEV_ATH_REGMON_H
#define _DEV_ATH_REGMON_H

#if !defined(AUTOCONF_INCLUDED) && !defined(CONFIG_LOCALVERSION)
#include <linux/config.h>
#endif
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/cache.h>
#include <linux/bitops.h>
#include <linux/types.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>
#include <linux/time.h>
#include <asm/uaccess.h>

#include "if_ethersubr.h"   /* for ETHER_IS_MULTICAST */
#include "if_media.h"
#include "if_llc.h"

#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_monitor.h>
#include <net80211/ieee80211_rate.h>

#ifdef USE_HEADERLEN_RESV
#include <net80211/if_llc.h>
#endif

#include "net80211/if_athproto.h"
#include "if_athvar.h"


/* phantom packet detecor */
#define STATE_RX      1
#define STATE_SILENCE 2
#define STATE_STRANGE 3

/* how many samples does it take, for us to be sure that this really is a
   phantom packet */
#define GLOBAL_MAX 10

/* We have already decided to push the phantom packet. How many samples do
   we delay the push to find out the state AFTER the phantom pkt. */
#define DELAY_MAX 3
/*
 * <!> Important <!>
 * DELAY_MAX needs to be << GLOBAL_MAX, since we don't use a
 * phantom pkt queue for ready phantom pkts. We only got 1 ready spot and
 * if DELAY_MAX >= GLOBAL_MAX we might lose a phantom pkt by overwriting it
 * with a newer one.
 */


/* upper and lower bound */
#define UB 97
#define LB  1

/* phantom state ring buffer size */
#define PH_BUF_SIZE 16


struct ar5212_rx_status {
	u_int32_t data_len:12;
	u_int32_t more:1;
	u_int32_t decomperr:2;
	u_int32_t rx_rate:5;
	u_int32_t rx_rssi:8;
	u_int32_t rx_ant:4;


	u_int32_t done:1;
	u_int32_t rx_ok:1;
	u_int32_t crcerr:1;
	u_int32_t decryptcrc:1;

} __attribute__((packed));


/* needed by the phantom paket detector for stats and current state */
struct phantom_state_info {
	/* state counts */
	u_int32_t rx_cnt;
	u_int32_t silence_cnt;
	u_int32_t strange_cnt;

	/* phantom mode indicator + stats count */
	u_int32_t pmode;
	u_int32_t pmode_cnt;

	u_int32_t curr_state;

	u_int32_t delay_cnt;
	u_int32_t is_delayed;
} __attribute__((packed));


struct ph_state_buf_entry {
	u_int32_t prev_state;
	u_int32_t curr_state;

	u_int64_t change_time;
} __attribute__((packed));


/* additional phantom data which is supposed to be put into a phantom pkt */
struct add_phantom_data {
	u_int32_t endianness;
	u_int32_t version;
	u_int32_t rb_size;    /* both, flag and size. if 0 -> no ring buffer */

	struct ph_state_buf_entry ph_rb[PH_BUF_SIZE];  /* ph state ringbuffer */
	u_int32_t ph_rb_index;

	u_int64_t ph_start;   /* start and end time of a phantom pkt in 'k_time' */
	u_int64_t ph_stop;
	u_int64_t ph_len;     /* packet length/duration in ns */
	u_int32_t next_state; /* state after ph pkt push */
} __attribute__((packed));


#ifdef BRN_REGMON_HR 
void check_rm_data_for_phantom_pkt(struct regmon_data * rmd, struct ath_softc *sc);
#endif

struct sk_buff *create_phantom_pkt(struct add_phantom_data *ph_data);

#endif
