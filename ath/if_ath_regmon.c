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

#include "if_ath_regmon.h"


#define DEBUG_SIZE 80
#define STR_RX     81
#define STR_SIL    82


#ifdef BRN_REGMON_HR
void check_rm_data_for_phantom_pkt(struct regmon_data * rmd, struct ath_softc *sc)
{
  u_int64_t phantom_len;
  struct sk_buff *skb = NULL;

  /* busy and rx values as percentages */
  u_int32_t val_busy = sc->channel_utility.busy;
  u_int32_t val_rx   = sc->channel_utility.rx;

  //if (val_busy != 0)
    //printk(KERN_ERR ">> busy: %d rx: %d\n", val_busy, val_rx);


  /* situations */
  u_int32_t rx      = 0;
  u_int32_t silence = 0;
  u_int32_t strange = 0;

  /* info */
  u_int32_t pmode      = sc->ph_state_info->pmode;
  u_int32_t curr_state = sc->ph_state_info->curr_state;


  rx      = (val_busy >= UB) && (val_rx >= UB);
  silence = (val_busy <= LB) && (val_rx <= LB);

  strange = ((val_busy > LB) && (val_busy < UB) && (val_rx < LB)) ||
            ((val_busy > LB) && (val_busy < UB) && (val_rx > LB) && (val_rx < UB)) ||
            ((val_busy > UB) && (val_rx < LB));


  sc->ph_state_info->debug++;

/*
  if (sc->ph_state_info->debug == 20000) {
      skb = create_phantom_pkt(DEBUG_SIZE);
      ieee80211_input_monitor(&sc->sc_ic, skb, sc->phantom_bf, 0, 0, sc);  // 1st 0 -> RX, 2nd 0 -> mac time
      printk(KERN_ERR ">> debug pkt\n");

      sc->ph_state_info->debug = 0;
  }
*/

  /* silence -> strange */
  if (!pmode && curr_state == STATE_SILENCE && strange) {

    /* mark phantom pkt starting point/index */
    sc->phantom_start = sc->regm_info->value.info.index;

    sc->ph_state_info->pmode = 1;
    sc->ph_state_info->pmode_cnt++;

    sc->ph_state_info->curr_state = STATE_STRANGE;
    sc->ph_state_info->strange_cnt++;

    //printk(KERN_ERR ">> started phantom pkt: silence -> strange\n");

    return;
  }


  /* strange -> rx */
  if (pmode && curr_state == STATE_STRANGE && rx) {

    /* there really were a couple of consecutive strange-samples */
    if (sc->ph_state_info->strange_cnt >= GLOBAL_MAX) {

      /* push phantom pkt */
      phantom_len = rmd->hrtime.tv64 - sc->regm_data[sc->phantom_start].hrtime.tv64;
      rmd->value.regs.phantom_pkt_len = (u_int32_t) phantom_len;

      skb = create_phantom_pkt(STR_RX);
      ieee80211_input_monitor(&sc->sc_ic, skb, sc->phantom_bf, 0, 0, sc);  // 1st 0 -> RX, 2nd 0 -> mac time

      //printk(KERN_ERR ">> phantom strange -> rx: %d\n", sc->ph_state_info->strange_cnt);

      /* reset all stats */
      sc->ph_state_info->pmode     = 0;
      sc->ph_state_info->pmode_cnt = 0;

      sc->ph_state_info->rx_cnt      = 0;
      sc->ph_state_info->silence_cnt = 0;
      sc->ph_state_info->strange_cnt = 0;

      sc->ph_state_info->curr_state = STATE_RX;

      return;

    } else { /* just pass state_strange cuz of 'normal' rx */

      sc->ph_state_info->pmode     = 0;
      sc->ph_state_info->pmode_cnt = 0;

      sc->ph_state_info->rx_cnt++;
      sc->ph_state_info->strange_cnt = 0;

      sc->ph_state_info->curr_state = STATE_RX;

      return;
    }

  }


  /* strange -> silence */
  if (pmode && curr_state == STATE_STRANGE && silence) {


    /* there really were a couple of consecutive strange-samples */
    if (sc->ph_state_info->strange_cnt >= GLOBAL_MAX) {

      /* push phantom pkt */
      phantom_len = rmd->hrtime.tv64 - sc->regm_data[sc->phantom_start].hrtime.tv64;
      rmd->value.regs.phantom_pkt_len = (u_int32_t) phantom_len;

      skb = create_phantom_pkt(STR_SIL);
      ieee80211_input_monitor(&sc->sc_ic, skb, sc->phantom_bf, 0, 0, sc);  // 1st 0 -> RX, 2nd 0 -> mac time

      //printk(KERN_ERR ">> phantom strange -> silence: %d\n", sc->ph_state_info->strange_cnt);

      /* reset all stats */
      sc->ph_state_info->pmode     = 0;
      sc->ph_state_info->pmode_cnt = 0;

      sc->ph_state_info->rx_cnt      = 0;
      sc->ph_state_info->silence_cnt = 0;
      sc->ph_state_info->strange_cnt = 0;

      sc->ph_state_info->curr_state = STATE_SILENCE;

      return;

    } else { /* just pass state_strange cuz of silence */

      sc->ph_state_info->pmode     = 0;
      sc->ph_state_info->pmode_cnt = 0;

      sc->ph_state_info->silence_cnt++;
      sc->ph_state_info->strange_cnt = 0;

      sc->ph_state_info->curr_state = STATE_SILENCE;

      return;
    }

  }


  /* rx -> silence */
  if (!pmode && curr_state == STATE_RX && silence) {

    sc->ph_state_info->rx_cnt = 0;
    sc->ph_state_info->silence_cnt++;

    sc->ph_state_info->curr_state = STATE_SILENCE;

    //printk(KERN_ERR ">> rx -> silence: %d\n", sc->ph_state_info->silence_cnt);

    return;
  }


  /* rx -> strange */
  if (!pmode && curr_state == STATE_RX && strange) {

    /* mark phantom pkt starting point/index */
    sc->phantom_start = sc->regm_info->value.info.index;

    sc->ph_state_info->pmode = 1;
    sc->ph_state_info->pmode_cnt++;

    sc->ph_state_info->curr_state = STATE_STRANGE;
    sc->ph_state_info->strange_cnt++;

    //printk(KERN_ERR ">> rx -> strange: %d\n", sc->ph_state_info->rx_cnt);

    return;
  }


  /* strange loops*/
  if (pmode && curr_state == STATE_STRANGE && strange) {
    sc->ph_state_info->strange_cnt++;
    return;
  }
if
   (!pmode && curr_state == STATE_STRANGE && strange) {
    sc->ph_state_info->strange_cnt++;
    return;
  }



  /* silence loops */
  if (pmode && curr_state == STATE_SILENCE && silence) {
    sc->ph_state_info->silence_cnt++;
    return;
  }

  if (!pmode && curr_state == STATE_SILENCE && silence) {
    sc->ph_state_info->silence_cnt++;
    return;
  }



  /* rx loops */
  if (!pmode && curr_state == STATE_RX && rx) {
    sc->ph_state_info->rx_cnt++;
    return;
  }

  if (!pmode && curr_state == STATE_RX && rx) {
    sc->ph_state_info->rx_cnt++;
    return;
  }


}
#endif


struct sk_buff *create_phantom_pkt(int pkt_size)
{
  struct ieee80211_frame *wh = NULL;
  unsigned int datasz        = 2048;  /* max. size = 2290 */
  char *data                 = NULL;


  struct sk_buff *skb = alloc_skb(sizeof(struct ieee80211_frame) + datasz +  IEEE80211_CRC_LEN, GFP_ATOMIC);

  if (NULL == skb) {
    printk(KERN_ERR "alloc_skb(...) returned null!\n");
  }

  data = (char *) skb_put(skb, pkt_size);
  memset(data, 0, pkt_size);

  wh  = (struct ieee80211_frame *)skb_put(skb, sizeof(struct ieee80211_frame));
  memset(wh, 0, sizeof(struct ieee80211_frame));


  return skb;
}
