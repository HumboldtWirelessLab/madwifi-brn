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

void check_rm_data_for_phantom_pkt(struct regmon_data * rmd, struct ath_softc *sc);
struct sk_buff *create_phantom_pkt(void);

#endif
