/*-
 * Copyright (c) 2005 John Bicket
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: ieee80211_monitor.c 3720 2008-06-12 05:22:37Z mentor $
 */

#ifndef EXPORT_SYMTAB
#define	EXPORT_SYMTAB
#endif

/*
 * IEEE 802.11 monitor mode 
 */
#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kmod.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/sysctl.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/if_vlan.h>

#include <net/iw_handler.h>
#include <linux/wireless.h>
#include <linux/if_arp.h>		/* for ARPHRD_* */

#include <asm/uaccess.h>

#include "if_media.h"
#include "if_ethersubr.h"

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_monitor.h>
#include <ath/if_athvar.h>

#ifdef CHANNEL_UTILITY
uint8_t get_channel_utility_busy(struct ath_softc *sc);
#endif

static int
ratecode_to_dot11(int ratecode)
{
	switch (ratecode) {
	    /* a */
	case 0x0b: return 12;
	case 0x0f: return 18;
	case 0x0a: return 24;
	case 0x0e: return 36;
	case 0x09: return 48;
	case 0x0d: return 72;
	case 0x08: return 96;
	case 0x0c: return 108;

	case 0x1b: return 2;
	case 0x1a: return 4;
	case 0x1e: return 4;
	case 0x19: return 11;
	case 0x1d: return 11;
	case 0x18: return 22;
	case 0x1c: return 22;
	}
	return 0;
}

struct ar5212_openbsd_desc {
	/*
	 * tx_control_0
	 */
	u_int32_t frame_len:12;
	u_int32_t reserved_12_15:4;
	u_int32_t xmit_power:6;
	u_int32_t rts_cts_enable:1;
	u_int32_t veol:1;
	u_int32_t clear_dest_mask:1;
	u_int32_t ant_mode_xmit:4;
	u_int32_t inter_req:1;
	u_int32_t encrypt_key_valid:1;
	u_int32_t cts_enable:1;

#ifndef EXTATHFLAGS
	u_int32_t r1;
#else
	/* the struct is cpoied from click/elements/wifi/athdesc.h */
	/*
	 * tx_control_1
	 */
	u_int32_t	buf_len:12;
	u_int32_t	more:1;
	u_int32_t	encrypt_key_index:7;
	u_int32_t	frame_type:4;
	u_int32_t	no_ack:1;
	u_int32_t	comp_proc:2;
	u_int32_t	comp_iv_len:2;
	u_int32_t	comp_icv_len:2;
	u_int32_t	reserved_31:1;
#endif

	/*
	 * tx_control_2
	 */
	u_int32_t rts_duration:15;
	u_int32_t duration_update_enable:1;
	u_int32_t xmit_tries0:4;
	u_int32_t xmit_tries1:4;
	u_int32_t xmit_tries2:4;
	u_int32_t xmit_tries3:4;

	/*
	 * tx_control_3
	 */
	u_int32_t xmit_rate0:5;
	u_int32_t xmit_rate1:5;
	u_int32_t xmit_rate2:5;
	u_int32_t xmit_rate3:5;
	u_int32_t rts_cts_rate:5;
	u_int32_t reserved_25_31:7;
};

void
ieee80211_monitor_encap(struct ieee80211vap *vap, struct sk_buff *skb)
{
	struct ieee80211_phy_params *ph = &(SKB_CB(skb)->phy);
	SKB_CB(skb)->flags = M_RAW;
	SKB_NI(skb) = ieee80211_ref_node(vap->iv_bss);

	/* send at a static rate if it is configured */
	ph->rate[0] = vap->iv_fixed_rate > 0 ? vap->iv_fixed_rate : 2;
	/* don't retry control packets */
	ph->try[0] = 11;
	ph->power = 60;

	switch (skb->dev->type) {
	case ARPHRD_IEEE80211: {
		struct ieee80211_frame *wh = (struct ieee80211_frame *)skb->data;
		if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_CTL)
			ph->try[0] = 1;
		break;
	}
	case ARPHRD_IEEE80211_PRISM: {
		struct ieee80211_frame *wh = NULL;
		struct wlan_ng_prism2_header *p2h =
			(struct wlan_ng_prism2_header *)skb->data;
		/* does it look like there is a prism header here? */
		if (skb->len > sizeof(struct wlan_ng_prism2_header) &&
	                p2h->msgcode == DIDmsg_lnxind_wlansniffrm &&
		    p2h->rate.did == DIDmsg_lnxind_wlansniffrm_rate) {
	                    ph->rate[0] = p2h->rate.data;
	                    skb_pull(skb, sizeof(struct wlan_ng_prism2_header));
		}
		wh = (struct ieee80211_frame *)skb->data;
		if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_CTL)
			ph->try[0] = 1;
		break;
	}
	case ARPHRD_IEEE80211_RADIOTAP: {
		struct ieee80211_frame *wh = NULL;
		struct ieee80211_radiotap_header *rh =
			(struct ieee80211_radiotap_header *)skb->data;
		u_int32_t present, present_ext;
		u_int16_t len;
		u_int8_t *start = skb->data + sizeof(struct ieee80211_radiotap_header);
		u_int8_t *p = start;
		u_int8_t *end = skb->data + skb->len;
		u_int8_t bit, flags = 0;

		if (skb->len < sizeof(*rh) || rh->it_version != 0)
			break;

		present_ext = present = le32_to_cpu(rh->it_present);
		len = le16_to_cpu(rh->it_len);

		if (skb->len < len)
			break;

		/* skip the chain of additional bitmaps following it_present */
		while (present_ext & (1 << IEEE80211_RADIOTAP_EXT)) {
			if (p + 4 > end) {
				/* An extended bitmap would now follow, but there is 
				 * no place for it. Stop parsing. */
				present = 0;
				break;
			}
			present_ext = le32_to_cpu(*(__le32 *)p);
			p += 4;
		}

		for (bit = 0; present && p < end; present >>= 1, bit++) {
			if ((present & 1) == 0)
				continue;
			switch (bit) {
				case IEEE80211_RADIOTAP_RATE:
					ph->rate[0] = *p;
					p++;
					break;

				case IEEE80211_RADIOTAP_DBM_TX_POWER:
					ph->power = *p;
					p++;
					break;

				case IEEE80211_RADIOTAP_FLAGS:
					flags = *p;
					p++;
					break;

				case IEEE80211_RADIOTAP_DB_ANTSIGNAL:
				case IEEE80211_RADIOTAP_DB_ANTNOISE:
				case IEEE80211_RADIOTAP_ANTENNA:
				case IEEE80211_RADIOTAP_DBM_ANTSIGNAL:
				case IEEE80211_RADIOTAP_DBM_ANTNOISE:
					/* 8-bit */
					p++;
					break;

				case IEEE80211_RADIOTAP_FHSS:
					/* 2 x 8-bit */
					p += 2;
					break;

				case IEEE80211_RADIOTAP_LOCK_QUALITY:
				case IEEE80211_RADIOTAP_TX_ATTENUATION:
				case IEEE80211_RADIOTAP_DB_TX_ATTENUATION:
					/* 16-bit */
					p = start + roundup(p - start, 2) + 2;
					break;

				case IEEE80211_RADIOTAP_CHANNEL:
					/* 2 x 16-bit */
					p = start + roundup(p - start, 2) + 4;
					break;

				case IEEE80211_RADIOTAP_TSFT:
					/* 64-bit */
					p = start + roundup(p - start, 8) + 8;
					break;

				case IEEE80211_RADIOTAP_DATA_RETRIES:
					ph->try[0] = *p + 1;
					p++;
					break;

				default:
					present = 0;
					break;
			}
		}
		skb_pull(skb, len);
		if (flags & IEEE80211_RADIOTAP_F_FCS)
			/* Remove FCS from the end of frames to transmit */
			skb_trim(skb, skb->len - IEEE80211_CRC_LEN);
		wh = (struct ieee80211_frame *)skb->data;
		if (!ph->try[0] &&
		    (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_CTL)
			ph->try[0] = 1;
		break;
	}
#ifdef AHT2HEADER
	case ARPHRD_IEEE80211_ATHDESC2:
#endif
	case ARPHRD_IEEE80211_ATHDESC: {
		if (skb->len > ATHDESC_HEADER_SIZE) {
			struct ar5212_openbsd_desc *desc =
				(struct ar5212_openbsd_desc *)(skb->data + 8);
			ph->power = desc->xmit_power;
			ph->rate[0] = ratecode_to_dot11(desc->xmit_rate0);
			ph->rate[1] = ratecode_to_dot11(desc->xmit_rate1);
			ph->rate[2] = ratecode_to_dot11(desc->xmit_rate2);
			ph->rate[3] = ratecode_to_dot11(desc->xmit_rate3);
			ph->try[0] = desc->xmit_tries0;
			ph->try[1] = desc->xmit_tries1;
			ph->try[2] = desc->xmit_tries2;
			ph->try[3] = desc->xmit_tries3;

#ifdef AHT2HEADER			
#ifdef EXTATHFLAGS
#ifdef EXTATHFLAGSDEBUG
			printk("PhyFlag in Monitor: %d\n",ph->flags);
#endif

			if ( desc->no_ack == 1 ) {				//set no ack to phyflags
				ph->flags |= HAL_TXDESC_NOACK;
			}

			if ( desc->ant_mode_xmit > 0 ) {		         // move antmode to the highest 4 bits (not used now)
				ph->flags |= ( desc->ant_mode_xmit << 28 );   
			}
			
			if ( desc->rts_cts_enable == 1 ) {
				ph->flags |= HAL_TXDESC_RTSENA;
			} else {
				if ( desc->cts_enable == 1 ) {          //is this cts-to-self ? Enable only if no RTS/CTS
					ph->flags |= HAL_TXDESC_CTSENA;
				}
			}
			
			if ( desc->rts_cts_rate > 0 ) {
				ph->flags |= ( desc->rts_cts_rate << 23 );
			}
			
#ifdef EXTATHFLAGSDEBUG
			printk("New PhyFlag in Monitor: %d\n",ph->flags);
#endif
#endif			
			
			if ( skb->dev->type == ARPHRD_IEEE80211_ATHDESC2 )
				skb_pull(skb, ATHDESC2_HEADER_SIZE);
			else
#endif
				skb_pull(skb, ATHDESC_HEADER_SIZE); 
		}
		break;
	}
	default:
		break;
	}

	if (!ph->rate[0]) {
		ph->rate[0] = 0;
		ph->try[0] = 11;
	}
}
EXPORT_SYMBOL(ieee80211_monitor_encap);

/*
 * Context: softIRQ (tasklet)
 */
void
ieee80211_input_monitor(struct ieee80211com *ic, struct sk_buff *skb,
	const struct ath_buf *bf, int tx, u_int64_t mactime, struct ath_softc *sc)
{
	struct ieee80211vap *vap, *next;
	struct ath_desc *ds = bf->bf_desc;
	int noise = 0, antenna = 0, ieeerate = 0;
	u_int32_t rssi = 0;
	u_int8_t pkttype = 0;
#ifdef AHT2HEADER
	u_int8_t *skb1_data;
	struct ath2_header ath2_h;
#endif

	if (tx) {
		rssi = bf->bf_dsstatus.ds_txstat.ts_rssi;
		antenna = bf->bf_dsstatus.ds_txstat.ts_antenna;
		ieeerate = sc->sc_hwmap[bf->bf_dsstatus.ds_txstat.ts_rate].ieeerate;
	} else {
		rssi = bf->bf_dsstatus.ds_rxstat.rs_rssi;
		antenna = bf->bf_dsstatus.ds_rxstat.rs_antenna;
		ieeerate = sc->sc_hwmap[bf->bf_dsstatus.ds_rxstat.rs_rate].ieeerate;
	}

	noise = bf->bf_channoise;

	/* XXX locking */
	for (vap = TAILQ_FIRST(&ic->ic_vaps); vap != NULL; vap = next) {
		struct sk_buff *skb1;
		struct net_device *dev = vap->iv_dev;
		struct ieee80211_frame *wh = (struct ieee80211_frame *)skb->data;
		u_int8_t dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;

		next = TAILQ_NEXT(vap, iv_next);
		/* If we have rx'd an error frame... */
		if (!tx && bf->bf_dsstatus.ds_rxstat.rs_status != 0) {

			/* Discard PHY errors if necessary */
			if (bf->bf_dsstatus.ds_rxstat.rs_status & HAL_RXERR_PHY) {
				if (vap->iv_monitor_phy_errors == 0) continue;
			}

			/* Discard CRC errors if necessary */
			if (bf->bf_dsstatus.ds_rxstat.rs_status & HAL_RXERR_CRC) {
				if (vap->iv_monitor_crc_errors == 0) continue;
			}

			/* Accept PHY, CRC and decrypt errors. Discard the rest. */
			if (bf->bf_dsstatus.ds_rxstat.rs_status &~
					(HAL_RXERR_DECRYPT | HAL_RXERR_MIC |
					 HAL_RXERR_PHY | HAL_RXERR_CRC))
				continue;

			/* We can't use addr1 to determine direction at this point */
			pkttype = PACKET_HOST;
		} else {
			/* 
			 * The frame passed its CRC, so we can rely
			 * on the contents of the frame to set pkttype.
			 */
			if (tx)
				pkttype = PACKET_OUTGOING;
			else if (IEEE80211_IS_MULTICAST(wh->i_addr1)) {
				if (IEEE80211_ADDR_EQ(wh->i_addr1, dev->broadcast))
					pkttype = PACKET_BROADCAST;
				else
					pkttype = PACKET_MULTICAST;
			} else
				pkttype = PACKET_HOST;
		}

		if (vap->iv_opmode != IEEE80211_M_MONITOR ||
		    vap->iv_state != IEEE80211_S_RUN)
			continue;
		if (vap->iv_monitor_nods_only &&
		    dir != IEEE80211_FC1_DIR_NODS) {
			/* don't rx fromds, tods, or dstods packets */
			continue;
		}
		
		if (skb_headroom(skb) < IEEE80211_MON_MAXHDROOM)
			skb1 = skb_copy_expand(skb, IEEE80211_MON_MAXHDROOM,
					0, GFP_ATOMIC);
		else
			skb1 = skb_copy(skb, GFP_ATOMIC);
		if (skb1 == NULL) {
			/* XXX stat+msg */
			continue;
		}
		ieee80211_skb_copy_noderef(skb, skb1);

		if (vap->iv_monitor_txf_len && tx) {
			/* truncate transmit feedback packets */
			skb_trim(skb1, vap->iv_monitor_txf_len);
			skb_reset_network_header(skb1);
		}
		switch (vap->iv_dev->type) {
		case ARPHRD_IEEE80211:
			break;
		case ARPHRD_IEEE80211_PRISM: {
			struct wlan_ng_prism2_header *ph;
			if (skb_headroom(skb1) < sizeof(struct wlan_ng_prism2_header)) {
				ieee80211_dev_kfree_skb(&skb1);
				break;
			}

			ph = (struct wlan_ng_prism2_header *)
				skb_push(skb1, sizeof(struct wlan_ng_prism2_header));
			memset(ph, 0, sizeof(struct wlan_ng_prism2_header));

			ph->msgcode = DIDmsg_lnxind_wlansniffrm;
			ph->msglen = sizeof(struct wlan_ng_prism2_header);
			strncpy(ph->devname, dev->name, sizeof(ph->devname));

			ph->hosttime.did = DIDmsg_lnxind_wlansniffrm_hosttime;
			ph->hosttime.status = 0;
			ph->hosttime.len = 4;
			ph->hosttime.data = jiffies;

			/* Pass up tsf clock in mactime */
			/* NB: the prism mactime field is 32bit, so we lose TSF precision here */
			ph->mactime.did = DIDmsg_lnxind_wlansniffrm_mactime;
			ph->mactime.status = 0;
			ph->mactime.len = 4;
			ph->mactime.data = mactime;

			ph->istx.did = DIDmsg_lnxind_wlansniffrm_istx;
			ph->istx.status = 0;
			ph->istx.len = 4;
			ph->istx.data = tx ? P80211ENUM_truth_true : P80211ENUM_truth_false;

			ph->frmlen.did = DIDmsg_lnxind_wlansniffrm_frmlen;
			ph->frmlen.status = 0;
			ph->frmlen.len = 4;
			ph->frmlen.data = skb->len;

			ph->channel.did = DIDmsg_lnxind_wlansniffrm_channel;
			ph->channel.status = 0;
			ph->channel.len = 4;
			ph->channel.data =
				ieee80211_mhz2ieee(ic->ic_curchan->ic_freq,
					ic->ic_curchan->ic_flags);

			ph->rssi.did = DIDmsg_lnxind_wlansniffrm_rssi;
			ph->rssi.status = 0;
			ph->rssi.len = 4;
			ph->rssi.data = rssi;

			ph->noise.did = DIDmsg_lnxind_wlansniffrm_noise;
			ph->noise.status = 0;
			ph->noise.len = 4;
			ph->noise.data = noise;

			ph->signal.did = DIDmsg_lnxind_wlansniffrm_signal;
			ph->signal.status = 0;
			ph->signal.len = 4;
			ph->signal.data = rssi + noise;

			ph->rate.did = DIDmsg_lnxind_wlansniffrm_rate;
			ph->rate.status = 0;
			ph->rate.len = 4;
			ph->rate.data = ieeerate;
			break;
		}
		case ARPHRD_IEEE80211_RADIOTAP: {
			if (tx) {
				struct ath_tx_radiotap_header *th;
				if (skb_headroom(skb1) < sizeof(struct ath_tx_radiotap_header)) {
					printk("%s:%d %s\n", __FILE__, __LINE__, __func__);
					ieee80211_dev_kfree_skb(&skb1);
					break;
				}

				th = (struct ath_tx_radiotap_header *)skb_push(skb1,
					sizeof(struct ath_tx_radiotap_header));
				memset(th, 0, sizeof(struct ath_tx_radiotap_header));
				th->wt_ihdr.it_version = 0;
				th->wt_ihdr.it_len = cpu_to_le16(sizeof(struct ath_tx_radiotap_header));
				th->wt_ihdr.it_present = cpu_to_le32(ATH_TX_RADIOTAP_PRESENT);

				/* radiotap's TSF field is the full 64 bits, so we don't lose
				 * any TSF precision when using radiotap */
				th->wt_tsft = cpu_to_le64(mactime);

				th->wt_flags = 0;
				th->wt_rate = ieeerate;
				th->wt_antenna = antenna;
				th->wt_pad = 0;

				if (bf->bf_dsstatus.ds_txstat.ts_status & HAL_TXERR_XRETRY)
					th->wt_txflags |= cpu_to_le16(IEEE80211_RADIOTAP_F_TX_FAIL);

				th->wt_dataretries = bf->bf_dsstatus.ds_txstat.ts_shortretry + bf->bf_dsstatus.ds_txstat.ts_longretry;

			} else {
				struct ath_rx_radiotap_header *th;
				if (skb_headroom(skb1) < sizeof(struct ath_rx_radiotap_header)) {
					printk("%s:%d %s\n", __FILE__, __LINE__, __func__);
					ieee80211_dev_kfree_skb(&skb1);
					break;
				}

				th = (struct ath_rx_radiotap_header *)skb_push(skb1,
					sizeof(struct ath_rx_radiotap_header));
				memset(th, 0, sizeof(struct ath_rx_radiotap_header));
				th->wr_ihdr.it_version = 0;
				th->wr_ihdr.it_len = cpu_to_le16(sizeof(struct ath_rx_radiotap_header));
				th->wr_ihdr.it_present = cpu_to_le32(ATH_RX_RADIOTAP_PRESENT);

				if (ic->ic_flags & IEEE80211_F_SHPREAMBLE)
					th->wr_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
				if (bf->bf_dsstatus.ds_rxstat.rs_status & HAL_RXERR_CRC)
					th->wr_flags |= IEEE80211_RADIOTAP_F_BADFCS;
				if (skb->len >= IEEE80211_CRC_LEN)
					th->wr_flags |= IEEE80211_RADIOTAP_F_FCS;

				th->wr_rate = ieeerate;
				th->wr_chan_freq = cpu_to_le16(ic->ic_curchan->ic_freq);

				/* Define the channel flags for radiotap */
				switch (sc->sc_curmode) {
					case IEEE80211_MODE_11A:
						th->wr_chan_flags =
							cpu_to_le16(IEEE80211_CHAN_A);
						break;
					case IEEE80211_MODE_TURBO_A:
						th->wr_chan_flags =
							cpu_to_le16(IEEE80211_CHAN_TA);
						break;
					case IEEE80211_MODE_11B:
						th->wr_chan_flags =
							cpu_to_le16(IEEE80211_CHAN_B);
						break;
					case IEEE80211_MODE_11G:
						th->wr_chan_flags =
							cpu_to_le16(IEEE80211_CHAN_G);
						break;
					case IEEE80211_MODE_TURBO_G:
						th->wr_chan_flags =
							cpu_to_le16(IEEE80211_CHAN_TG);
						break;
					default:
						th->wr_chan_flags = 0; /* unknown */
						break;
				}

				th->wr_dbm_antnoise = (int8_t) noise;
				th->wr_dbm_antsignal = 
					th->wr_dbm_antnoise + rssi;
				th->wr_antenna = antenna;
				th->wr_antsignal = rssi;

				th->wr_tsft = cpu_to_le64(mactime);
			}
			break;
		}
		case ARPHRD_IEEE80211_ATHDESC: {
			if (skb_headroom(skb1) < ATHDESC_HEADER_SIZE) {
				printk("%s:%d %s\n", __FILE__, 
						__LINE__, __func__);
				ieee80211_dev_kfree_skb(&skb1);
				break;
			}
			memcpy(skb_push(skb1, ATHDESC_HEADER_SIZE), 
					ds, ATHDESC_HEADER_SIZE);
			break;
		}
#ifdef AHT2HEADER
		case ARPHRD_IEEE80211_ATHDESC2: {
			if (skb_headroom(skb1) < ATHDESC2_HEADER_SIZE) {
				printk("%s:%d %s\n", __FILE__, 
						__LINE__, __func__);
				ieee80211_dev_kfree_skb(&skb1);
				break;
			}
			
			ath2_h.ath2_version = cpu_to_le16(ATHDESC2_VERSION);
			ath2_h.madwifi_version = cpu_to_le16(MADWIFI_TRUNK);
			
			/* Flags */
#ifdef COLORADO_CCA
			ath2_h.flags = sc->sc_disable_cca_mask;
#else			
			ath2_h.flags = 0;	
#endif

#ifdef CHANNELSWITCH
			 if ( (vap->iv_flags_ext & IEEE80211_FEXT_CHANNELSWITCH) != 0 )
			   ath2_h.flags |= MADWIFI_FLAGS_CHANNELSWITCH_ENABLED;
#endif

#ifdef MACCLONE
                        if ( (vap->iv_flags_ext & IEEE80211_FEXT_MACCLONE) != 0 )
			   ath2_h.flags |= MADWIFI_FLAGS_MACCLONE_ENABLED;
#endif

			ath2_h.flags = cpu_to_le32(ath2_h.flags);
			
			if (tx)
			{
				ath2_h.anno.tx.ts_seqnum = bf->bf_dsstatus.ds_txstat.ts_seqnum;
				ath2_h.anno.tx.ts_tstamp = bf->bf_dsstatus.ds_txstat.ts_tstamp;
				ath2_h.anno.tx.ts_status = bf->bf_dsstatus.ds_txstat.ts_status;
				ath2_h.anno.tx.ts_rate = bf->bf_dsstatus.ds_txstat.ts_rate;
				ath2_h.anno.tx.ts_rssi = bf->bf_dsstatus.ds_txstat.ts_rssi;
				ath2_h.anno.tx.ts_shortretry = bf->bf_dsstatus.ds_txstat.ts_shortretry;
				ath2_h.anno.tx.ts_longretry = bf->bf_dsstatus.ds_txstat.ts_longretry;
				ath2_h.anno.tx.ts_virtcol = bf->bf_dsstatus.ds_txstat.ts_virtcol;
				ath2_h.anno.tx.ts_antenna = bf->bf_dsstatus.ds_txstat.ts_antenna;
				ath2_h.anno.tx.ts_finaltsi = bf->bf_dsstatus.ds_txstat.ts_finaltsi;

				ath2_h.anno.tx.ts_hosttime = jiffies;
				ath2_h.anno.tx.ts_mactime = mactime;
				ath2_h.anno.tx.ts_noise = (int8_t) noise;
				ath2_h.anno.tx.ts_channel = (int8_t) ieee80211_mhz2ieee(ic->ic_curchan->ic_freq, ic->ic_curchan->ic_flags);
				ath2_h.anno.tx.ts_flags = 0;
#ifdef CHANNEL_UTILITY
				if ( sc->cc_survey.cycles == 0 ) ath2_h.anno.tx.ts_channel_utility = 0;
				else ath2_h.anno.tx.ts_channel_utility = (sc->cc_survey.rx_busy)/(sc->cc_survey.cycles/100);
#else
				ath2_h.anno.tx.ts_channel_utility = 0;
#endif
			}
			else
			{
				ath2_h.anno.rx.rs_datalen = bf->bf_dsstatus.ds_rxstat.rs_datalen;
				ath2_h.anno.rx.rs_status = bf->bf_dsstatus.ds_rxstat.rs_status;
				ath2_h.anno.rx.rs_phyerr = bf->bf_dsstatus.ds_rxstat.rs_phyerr;
				ath2_h.anno.rx.rs_rssi = bf->bf_dsstatus.ds_rxstat.rs_rssi;
				ath2_h.anno.rx.rs_keyix = bf->bf_dsstatus.ds_rxstat.rs_keyix;
				ath2_h.anno.rx.rs_rate = bf->bf_dsstatus.ds_rxstat.rs_rate;
				ath2_h.anno.rx.rs_more = bf->bf_dsstatus.ds_rxstat.rs_more;
				ath2_h.anno.rx.rs_tstamp = bf->bf_dsstatus.ds_rxstat.rs_tstamp;
				ath2_h.anno.rx.rs_antenna = bf->bf_dsstatus.ds_rxstat.rs_antenna;

				ath2_h.anno.rx.rs_hosttime = jiffies;
				ath2_h.anno.rx.rs_mactime = mactime;
				ath2_h.anno.rx.rs_noise = (int8_t) noise;
				ath2_h.anno.rx.rs_channel = (int8_t) ieee80211_mhz2ieee(ic->ic_curchan->ic_freq, ic->ic_curchan->ic_flags);
				
				if (ic->ic_flags & IEEE80211_F_SHPREAMBLE) {
					ath2_h.anno.rx.rs_flags = MADWIFI_RXTX_FLAGS_SHORT_PREAMBLE;
				} else {
					ath2_h.anno.rx.rs_flags = 0;
				}
#ifdef CHANNEL_UTILITY
				if ( sc->cc_survey.cycles == 0 ) ath2_h.anno.rx.rs_channel_utility = 0;
				else ath2_h.anno.rx.rs_channel_utility = (sc->cc_survey.rx_busy)/(sc->cc_survey.cycles/100);
#else
				ath2_h.anno.rx.rs_channel_utility = 0;
#endif
			}

			skb1_data = skb_push(skb1, ATHDESC2_HEADER_SIZE);
			
			memcpy(skb1_data, ds, ATHDESC_HEADER_SIZE);
			memcpy(&(skb1_data[ATHDESC_HEADER_SIZE]), &(ath2_h), ATHDESC2_BRN_HEADER_SIZE );
			/*Copy the hole rx/tx-status and brn extra*/
			break;
		}
#endif
		default:
			break;
		}
		if (skb1 != NULL) {
			if (!tx && (skb1->len >= IEEE80211_CRC_LEN) && 
					(vap->iv_dev->type != 
					 ARPHRD_IEEE80211_RADIOTAP)) {
				/* Remove FCS from end of RX frames when
				 * delivering to non-Radiotap VAPs. */
				skb_trim(skb1, skb1->len - IEEE80211_CRC_LEN);
			}
			skb1->dev = dev; /* NB: deliver to wlanX */
			skb_reset_mac_header(skb1);

			skb1->ip_summed = CHECKSUM_NONE;
			skb1->pkt_type = pkttype;
			skb1->protocol = 
				__constant_htons(0x0019); /* ETH_P_80211_RAW */

			vap->iv_devstats.rx_packets++;
			vap->iv_devstats.rx_bytes += skb1->len;

			if (SKB_NI(skb1) != NULL)
				ieee80211_unref_node(&SKB_NI(skb1));
			if (netif_rx(skb1) == NET_RX_DROP)
				vap->iv_devstats.rx_dropped++;
			skb1 = NULL;
		}
	}
}
EXPORT_SYMBOL(ieee80211_input_monitor);
