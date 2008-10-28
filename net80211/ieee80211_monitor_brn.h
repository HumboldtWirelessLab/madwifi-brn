#ifndef _NET80211_IEEE80211_MONITOR_BRN_H_
#define _NET80211_IEEE80211_MONITOR_BRN_H_

#include <net80211/ieee80211_monitor.h>
#include <ah_desc.h>
#include <ath/if_athvar.h>

struct ath_brn_rx_info {
    int8_t noise;
    u_int64_t hosttime;
    u_int64_t mactime;
};

struct ath_brn_tx_info {
    int8_t channel;
    u_int8_t mac[6];
};

struct ath_brn_info {
    u_int16_t id;
    union {
	struct ath_brn_rx_info rx;
	struct ath_brn_tx_info tx;
    } anno; 
};

#define ATHDESC2_BRN_ID 0xF2F2

#ifndef ARPHRD_IEEE80211_ATHDESC2
#define ARPHRD_IEEE80211_ATHDESC2  805 /* IEEE 802.11 + atheros (long) descriptor */
#endif /* ARPHRD_IEEE80211_ATHDESC2 */

#define ATHDESC2_EXTRA_HEADER_SIZE sizeof(struct ath_desc_status)
#define ATHDESC2_BRN_HEADER_SIZE sizeof(struct ath_brn_info)
#define ATHDESC2_HEADER_SIZE	( ATHDESC_HEADER_SIZE + ATHDESC2_EXTRA_HEADER_SIZE + ATHDESC2_BRN_HEADER_SIZE ) 

#endif
