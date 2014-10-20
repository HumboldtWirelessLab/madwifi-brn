#ifndef BRN_H_INCLUDE
#define BRN_H_INCLUDE

#ifdef RELEASE_VERSION

#undef RELEASE_VERSION

#define RELEASE_VERSION "madwifi-brn 0.2.0.1"

#endif


#ifndef CONFIG_HIGH_RES_TIMERS
#undef BRN_REGMON_HR
#else
#pragma "Has HR"
#endif

#endif
