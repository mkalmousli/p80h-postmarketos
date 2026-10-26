/* SPDX-License-Identifier: GPL-2.0 */
/*
 * cfg80211 ops on linux 6.12: the key/station ops take the net_device (later kernels pass the wireless_dev) and
 * remain_on_channel has no rx_addr. Each macro defines fn##_612 with the 6.12 signature around the driver's fn.
 */
#ifndef _CFG80211_612_H
#define _CFG80211_612_H

#include <net/cfg80211.h>

#define CFG612_ADD_KEY(fn) \
static int fn##_612(struct wiphy *w, struct net_device *d, int link, u8 idx, bool pw, const u8 *mac, \
		    struct key_params *p) { return fn(w, d->ieee80211_ptr, link, idx, pw, mac, p); }
#define CFG612_GET_KEY(fn) \
static int fn##_612(struct wiphy *w, struct net_device *d, int link, u8 idx, bool pw, const u8 *mac, void *c, \
		    void (*cb)(void *, struct key_params *)) { return fn(w, d->ieee80211_ptr, link, idx, pw, mac, c, cb); }
#define CFG612_DEL_KEY(fn) \
static int fn##_612(struct wiphy *w, struct net_device *d, int link, u8 idx, bool pw, const u8 *mac) \
		    { return fn(w, d->ieee80211_ptr, link, idx, pw, mac); }
#define CFG612_KEY_IDX(fn) \
static int fn##_612(struct wiphy *w, struct net_device *d, int link, u8 idx) \
		    { return fn(w, d->ieee80211_ptr, link, idx); }
#define CFG612_GET_STATION(fn) \
static int fn##_612(struct wiphy *w, struct net_device *d, const u8 *mac, struct station_info *s) \
		    { return fn(w, d->ieee80211_ptr, mac, s); }
#define CFG612_DUMP_STATION(fn) \
static int fn##_612(struct wiphy *w, struct net_device *d, int i, u8 *mac, struct station_info *s) \
		    { return fn(w, d->ieee80211_ptr, i, mac, s); }
#define CFG612_STATION_PARAMS(fn) \
static int fn##_612(struct wiphy *w, struct net_device *d, const u8 *mac, struct station_parameters *p) \
		    { return fn(w, d->ieee80211_ptr, mac, p); }
#define CFG612_DEL_STATION(fn) \
static int fn##_612(struct wiphy *w, struct net_device *d, struct station_del_parameters *p) \
		    { return fn(w, d->ieee80211_ptr, p); }
#define CFG612_REMAIN_ON_CHANNEL(fn) \
static int fn##_612(struct wiphy *w, struct wireless_dev *wd, struct ieee80211_channel *ch, unsigned int ms, \
		    u64 *cookie) { return fn(w, wd, ch, ms, cookie, NULL); }

#define CFG612_WIPHY_PARAMS(fn) \
static int fn##_612(struct wiphy *w, u32 changed) { return fn(w, 0, changed); }
#define CFG612_SET_TXPOWER(fn) \
static int fn##_612(struct wiphy *w, struct wireless_dev *wd, enum nl80211_tx_power_setting t, int mbm) \
		    { return fn(w, wd, 0, t, mbm); }
#define CFG612_GET_TXPOWER(fn) \
static int fn##_612(struct wiphy *w, struct wireless_dev *wd, int *dbm) { return fn(w, wd, 0, 0, dbm); }

#endif
