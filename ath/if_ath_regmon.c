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
  u_int64_t phantom_len;
  struct sk_buff *skb = NULL;
  //u_int32_t busy_percentage = sc->channel_utility.busy;
  //u_int32_t rx_percentage   = sc->channel_utility.rx;
  rmd->value.regs.phantom_pkt_len = 0;

#ifdef BLABLA
  /* channel is busy but we're not receiving */
  if (busy_percentage > 0 && busy_percentage < 100 && rx_percentage < busy_percentage) { /* BUG: RX? */
    sc->phantom_cnt++;

    if (sc->phantom_start == 0) /* mark start of 'phantom pkt' */
      sc->phantom_start = sc->regm_info->value.info.index;

    /* normal RX, create phantom pkt and reset */
  } else if (busy_percentage == 100 && rx_percentage == 100) {
    sc->phantom_cnt = 0;

    if (sc->phantom_start != 0) {
      u_int64_t phantom_len = rmd->hrtime.tv64 - sc->regm_data[sc->phantom_start].hrtime.tv64;
      rmd->value.regs.phantom_pkt_len = phantom_len;

      sc->phantom_start = 0;
    }

    /* channel is quiet but earlier we recognized ACI */
  } else if (busy_percentage == 0 && sc->phantom_cnt > 0) {
#endif
    sc->phantom_cnt = 0;

    phantom_len = rmd->hrtime.tv64 - sc->regm_data[sc->phantom_start].hrtime.tv64;
    rmd->value.regs.phantom_pkt_len = (u_int32_t) phantom_len;

    //sc->phantom_start = 0;
    sc->phantom_start++;

    if ( sc->phantom_start == 1000 ) {

      printk(KERN_ERR "create phan\n");
      skb = create_phantom_pkt();
      printk(KERN_ERR "done. pkt to input_mon\n");
      ieee80211_input_monitor(&sc->sc_ic, skb, sc->phantom_bf, 0, 0, sc);  // 0 -> RX
      printk(KERN_ERR "FIN\n");

      sc->phantom_start = 0;
    }

  //}
}
#endif

struct sk_buff *create_phantom_pkt(void)
{
  struct ieee80211_frame *wh = NULL;
  unsigned int datasz        = 2048;
  char *data                 = NULL;


  struct sk_buff *skb = alloc_skb(sizeof(struct ieee80211_frame) + datasz +  IEEE80211_CRC_LEN, GFP_ATOMIC);

  if (NULL == skb) {
    printk(KERN_ERR "alloc_skb(...) returned null!\n");
  }

  data = (char *) skb_put(skb, 80);
  memset(data, 0, 80);

  wh  = (struct ieee80211_frame *)skb_put(skb, sizeof(struct ieee80211_frame));
  memset(wh, 0, sizeof(struct ieee80211_frame));


  return skb;
}
