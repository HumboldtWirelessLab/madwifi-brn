#ifndef _NET80211_IEEE80211_MONITOR_BRN_H_
#define _NET80211_IEEE80211_MONITOR_BRN_H_

#include <net80211/ieee80211_monitor.h>
#include <ah_desc.h>
#include <ath/if_athvar.h>

#include <net80211/ieee80211_monitor_ath2.h>

#ifndef ARPHRD_IEEE80211_ATHDESC2
#define ARPHRD_IEEE80211_ATHDESC2  805 /* IEEE 802.11 + atheros (long) descriptor */
#endif /* ARPHRD_IEEE80211_ATHDESC2 */

#define ATHDESC2_BRN_HEADER_SIZE sizeof(struct ath2_header)
#define ATHDESC2_HEADER_SIZE	( ATHDESC_HEADER_SIZE + ATHDESC2_BRN_HEADER_SIZE ) 

#endif
