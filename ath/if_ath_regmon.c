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



#ifdef BRN_REGMON_HR
void check_rm_data_for_phantom_pkt(struct regmon_data * rmd, struct ath_softc *sc)
{
  struct sk_buff *skb = NULL;

  /* busy and rx values as percentages */
  u_int32_t val_busy = sc->channel_utility.busy;
  u_int32_t val_rx   = sc->channel_utility.rx;

  /* situations */
  u_int32_t rx      = 0;
  u_int32_t silence = 0;
  u_int32_t strange = 0;

  /* info */
  u_int32_t pmode      = sc->ph_state_info->pmode;
  u_int32_t curr_state = sc->ph_state_info->curr_state;

  /* phantom state ringbuffer index */
  u_int32_t rb_index;

  rx      = (val_busy >= UB) && (val_rx >= UB);
  silence = (val_busy <= LB) && (val_rx <= LB);

  strange = ((val_busy > LB) && (val_busy < UB) && (val_rx < LB)) ||
            ((val_busy > LB) && (val_busy < UB) && (val_rx > LB) && (val_rx < UB)) ||
            ((val_busy > UB) && (val_rx < LB));

  rb_index= sc->ph_data->ph_rb_index;

#ifdef BRN_PHANTOM_PACKET_TEST
  if (sc->ph_state_info->is_delayed == 1)
    sc->ph_state_info->delay_cnt++;


  if (sc->ph_state_info->delay_cnt >= DELAY_MAX) {
    sc->rdy_ph_pkt->next_state = curr_state;

    /* push phantom packet */
    skb = create_phantom_pkt(sc->rdy_ph_pkt);
    ieee80211_input_monitor(&sc->sc_ic, skb, sc->phantom_bf, 0, 0, sc);  // 1st 0 -> RX, 2nd 0 -> mac time

    /* reset delay mechanism */
    sc->ph_state_info->delay_cnt  = 0;
    sc->ph_state_info->is_delayed = 0;
  }
#endif

  /* silence -> strange */
  if (!pmode && curr_state == STATE_SILENCE && strange) {

    /* mark phantom pkt starting point/index */
    sc->phantom_start = sc->regm_info->value.info.index;

    /* change state */
    sc->ph_state_info->pmode = 1;
    sc->ph_state_info->pmode_cnt++;
    sc->ph_state_info->curr_state = STATE_STRANGE;
    sc->ph_state_info->strange_cnt++;

    /* update phantom ring buffer */
    sc->ph_data->ph_rb[rb_index].prev_state  = STATE_SILENCE;
    sc->ph_data->ph_rb[rb_index].curr_state  = STATE_STRANGE;
    sc->ph_data->ph_rb[rb_index].change_time = rmd->hrtime.tv64;

    /* phantom ring buffer index update */
    sc->ph_data->ph_rb_index = (sc->ph_data->ph_rb_index + 1) & (PH_BUF_SIZE - 1);

    return;
  }


  /* strange -> rx */
  if (pmode && curr_state == STATE_STRANGE && rx) {

    /* there really were a couple of consecutive strange-samples */
    if (sc->ph_state_info->strange_cnt >= GLOBAL_MAX) {

      /* reset state machine stats */
      sc->ph_state_info->pmode     = 0;
      sc->ph_state_info->pmode_cnt = 0;

      sc->ph_state_info->rx_cnt      = 0;
      sc->ph_state_info->silence_cnt = 0;
      sc->ph_state_info->strange_cnt = 0;

      /* change state */
      sc->ph_state_info->curr_state = STATE_RX;
      sc->ph_state_info->rx_cnt++;

      /* update phantom ring buffer */
      sc->ph_data->ph_rb[rb_index].prev_state  = STATE_STRANGE;
      sc->ph_data->ph_rb[rb_index].curr_state  = STATE_RX;
      sc->ph_data->ph_rb[rb_index].change_time = rmd->hrtime.tv64;

      /* phantom ring buffer index update */
      sc->ph_data->ph_rb_index = (sc->ph_data->ph_rb_index + 1) & (PH_BUF_SIZE - 1);

      /* fill phantom pkt with addition info */
      sc->ph_data->ph_start = sc->regm_data[sc->phantom_start].hrtime.tv64;
      sc->ph_data->ph_stop  = rmd->hrtime.tv64;
      sc->ph_data->ph_len   = sc->ph_data->ph_start - sc->ph_data->ph_stop;

#ifdef BRN_PHANTOM_PACKET_TEST
      /* mark pkt as ready to push */
      memcpy(sc->rdy_ph_pkt, sc->ph_data, sizeof(struct add_phantom_data));
      sc->ph_state_info->is_delayed = 1;
#endif
    /* push phantom packet */
    skb = create_phantom_pkt(sc->ph_data);
    ieee80211_input_monitor(&sc->sc_ic, skb, sc->phantom_bf, 0, 0, sc);  // 1st 0 -> RX, 2nd 0 -> mac time

      /* reset phantom pkt stats */
      sc->phantom_start = 0;
      sc->phantom_cnt   = 0;

      /* legacy code, probably needs refactoring */
      rmd->value.regs.phantom_pkt_len = (u_int32_t) sc->ph_data->ph_len;

      return;

    } else { /* just normal rx */

      /* reset state machine stats */
      sc->ph_state_info->pmode       = 0;
      sc->ph_state_info->pmode_cnt   = 0;
      sc->ph_state_info->strange_cnt = 0;

      /* change state */
      sc->ph_state_info->curr_state = STATE_RX;
      sc->ph_state_info->rx_cnt++;

      /* update phanom ring buffer */
      sc->ph_data->ph_rb[rb_index].prev_state  = STATE_STRANGE;
      sc->ph_data->ph_rb[rb_index].curr_state  = STATE_RX;
      sc->ph_data->ph_rb[rb_index].change_time = rmd->hrtime.tv64;

      /* update phantom ring buffer index */
      sc->ph_data->ph_rb_index = (sc->ph_data->ph_rb_index + 1) & (PH_BUF_SIZE - 1);

      return;
    }

  }


  /* strange -> silence */
  if (pmode && curr_state == STATE_STRANGE && silence) {


    /* there really were a couple of consecutive strange-samples */
    if (sc->ph_state_info->strange_cnt >= GLOBAL_MAX) {

      /* reset state machine stats */
      sc->ph_state_info->pmode     = 0;
      sc->ph_state_info->pmode_cnt = 0;

      sc->ph_state_info->rx_cnt      = 0;
      sc->ph_state_info->silence_cnt = 0;
      sc->ph_state_info->strange_cnt = 0;

      /* change state */
      sc->ph_state_info->curr_state = STATE_SILENCE;
      sc->ph_state_info->silence_cnt++;

      /* update phantom ring buffer */
      sc->ph_data->ph_rb[rb_index].prev_state  = STATE_STRANGE;
      sc->ph_data->ph_rb[rb_index].curr_state  = STATE_SILENCE;
      sc->ph_data->ph_rb[rb_index].change_time = rmd->hrtime.tv64;

      /* update phantom ring buffer index */
      sc->ph_data->ph_rb_index = (sc->ph_data->ph_rb_index + 1) & (PH_BUF_SIZE - 1);

      /* fill phantom pkt with addition info */
      sc->ph_data->ph_start = sc->regm_data[sc->phantom_start].hrtime.tv64;
      sc->ph_data->ph_stop  = rmd->hrtime.tv64;
      sc->ph_data->ph_len   = sc->ph_data->ph_start - sc->ph_data->ph_stop;

#ifdef BRN_PHANTOM_PACKET_TEST
      /* mark pkt as ready to push */
      memcpy(sc->rdy_ph_pkt, sc->ph_data, sizeof(struct add_phantom_data));
      sc->ph_state_info->is_delayed = 1;
#endif

    /* push phantom packet */
    skb = create_phantom_pkt(sc->ph_data);
    ieee80211_input_monitor(&sc->sc_ic, skb, sc->phantom_bf, 0, 0, sc);  // 1st 0 -> RX, 2nd 0 -> mac time

      /* reset phantom pkt stats */
      sc->phantom_start = 0;
      sc->phantom_cnt  = 0;

      /* legacy code, probably needs refactoring */
      rmd->value.regs.phantom_pkt_len = (u_int32_t) sc->ph_data->ph_len;

      return;

    } else { /* just pass state_strange cuz of silence */

      /* reset state machine stats */
      sc->ph_state_info->pmode       = 0;
      sc->ph_state_info->pmode_cnt   = 0;
      sc->ph_state_info->strange_cnt = 0;

      /* change state */
      sc->ph_state_info->curr_state = STATE_SILENCE;
      sc->ph_state_info->silence_cnt++;

      /* update phanom ring buffer */
      sc->ph_data->ph_rb[rb_index].prev_state  = STATE_STRANGE;
      sc->ph_data->ph_rb[rb_index].curr_state  = STATE_SILENCE;
      sc->ph_data->ph_rb[rb_index].change_time = rmd->hrtime.tv64;

      /* update phantom ring buffer index */
      sc->ph_data->ph_rb_index = (sc->ph_data->ph_rb_index + 1) & (PH_BUF_SIZE - 1);

      return;
    }

  }


  /* rx -> silence */
  if (!pmode && curr_state == STATE_RX && silence) {

    /* reset state machine stats */
    sc->ph_state_info->rx_cnt = 0;

    /* change state */
    sc->ph_state_info->curr_state = STATE_SILENCE;
    sc->ph_state_info->silence_cnt++;

    /* update phanom ring buffer */
    sc->ph_data->ph_rb[rb_index].prev_state  = STATE_RX;
    sc->ph_data->ph_rb[rb_index].curr_state  = STATE_SILENCE;
    sc->ph_data->ph_rb[rb_index].change_time = rmd->hrtime.tv64;

    /* update phantom ring buffer index */
    sc->ph_data->ph_rb_index = (sc->ph_data->ph_rb_index + 1) & (PH_BUF_SIZE - 1);

    return;
  }


  /* rx -> strange */
  if (!pmode && curr_state == STATE_RX && strange) {

    /* mark phantom pkt starting point/index */
    sc->phantom_start = sc->regm_info->value.info.index;

    /* upate state machine stats */
    sc->ph_state_info->pmode = 1;
    sc->ph_state_info->pmode_cnt++;

    /* change state */
    sc->ph_state_info->curr_state = STATE_STRANGE;
    sc->ph_state_info->strange_cnt++;

    /* update phantom ring buffer */
    sc->ph_data->ph_rb[rb_index].prev_state  = STATE_RX;
    sc->ph_data->ph_rb[rb_index].curr_state  = STATE_STRANGE;
    sc->ph_data->ph_rb[rb_index].change_time = rmd->hrtime.tv64;

    /* phantom ring buffer index update */
    sc->ph_data->ph_rb_index = (sc->ph_data->ph_rb_index + 1) & (PH_BUF_SIZE - 1);

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


struct sk_buff *create_phantom_pkt(struct add_phantom_data *ph_data)
{
  struct ieee80211_frame *wh = NULL;
  unsigned int datasz        = 2048;  /* max. size = 2290 */
  char *data                 = NULL;


  struct sk_buff *skb = alloc_skb(sizeof(struct ieee80211_frame) + datasz +  IEEE80211_CRC_LEN, GFP_ATOMIC);

  if (NULL == skb) {
    printk(KERN_ERR "alloc_skb(...) returned null!\n");
  }


  /* skb = 20 Byte */
  data = (char *) skb_put(skb, sizeof(struct add_phantom_data));
  memset(data, 0, sizeof(struct add_phantom_data));

  memcpy(data, ph_data, sizeof(struct add_phantom_data));


  wh  = (struct ieee80211_frame *)skb_put(skb, sizeof(struct ieee80211_frame));
  memset(wh, 0, sizeof(struct ieee80211_frame));


  return skb;
}
