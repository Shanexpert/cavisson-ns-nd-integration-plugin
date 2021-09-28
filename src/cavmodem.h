/*
 *  include/linux/cavmodem.h
 *
 */


#ifndef _LINUX_CAVMODEM_H
#define _LINUX_CAVMODEM_H

#if ( (Fedora && RELEASE >= 14) || (Ubuntu && RELEASE >= 1204) )
//Below checks are commented as Ubuntu 1604 and above version will be used only.
/*
#if (Fedora && RELEASE >= 14)
#define TCP_BWEMU_OPT_BASE      18
*/
/*For Ubuntu 12.04, two kernels were installed-3.8 and 3.13 for which TCP_BWEMU_OPT_BASE was set as 24 and 26 respectively.
For Ubuntu 14.04, kernel 4.2 was installed for which TCP_BWEMU_OPT_BASE was set as 29. */
/*
#elif (Ubuntu && RELEASE == 1204)
#define TCP_BWEMU_OPT_BASE      24
#elif (Ubuntu && RELEASE == 1604)
#define TCP_BWEMU_OPT_BASE      29
#endif
*/
#define TCP_BWEMU_OPT_BASE      29
#define TCP_BWEMU_RPD           (TCP_BWEMU_OPT_BASE)
#define TCP_BWEMU_REV_RPD       (TCP_BWEMU_OPT_BASE + 1)
#define TCP_BWEMU_DELAY         (TCP_BWEMU_OPT_BASE + 2)
#define TCP_BWEMU_REV_DELAY     (TCP_BWEMU_OPT_BASE + 3)
#define TCP_BWEMU_COMPRESS      (TCP_BWEMU_OPT_BASE + 4)
#define TCP_BWEMU_REV_COMPRESS  (TCP_BWEMU_OPT_BASE + 5)

#define TCP_RTT                 (TCP_BWEMU_OPT_BASE + 6)
#define TCP_ESTM_SPD            (TCP_BWEMU_OPT_BASE + 7)

#define TCP_BWEMU_JITTER        (TCP_BWEMU_OPT_BASE + 8)
#define TCP_BWEMU_REV_JITTER    (TCP_BWEMU_OPT_BASE + 9)
#define TCP_BWEMU_SET_ALL       (TCP_BWEMU_OPT_BASE + 10)   /* See the net/sock.h for the all structure */
#define TCP_BWEMU_MODEM         (TCP_BWEMU_OPT_BASE + 11)
#define TCP_BWEMU_RELEASE_PAGE  (TCP_BWEMU_OPT_BASE + 12)

#else
// These are for FC 4/9
#define TCP_BWEMU_OPT_BASE      14
#define TCP_BWEMU_MODEM         (TCP_BWEMU_OPT_BASE)
#define TCP_BWEMU_RPD           (TCP_BWEMU_OPT_BASE + 1)
#define TCP_BWEMU_REV_RPD       (TCP_BWEMU_OPT_BASE + 2)
#define TCP_BWEMU_DELAY         (TCP_BWEMU_OPT_BASE + 3)
#define TCP_BWEMU_REV_DELAY     (TCP_BWEMU_OPT_BASE + 4)
#define TCP_BWEMU_COMPRESS      (TCP_BWEMU_OPT_BASE + 5)
#define TCP_BWEMU_REV_COMPRESS  (TCP_BWEMU_OPT_BASE + 6)

#define TCP_BWEMU_JITTER        (TCP_BWEMU_OPT_BASE + 9)
#define TCP_BWEMU_REV_JITTER    (TCP_BWEMU_OPT_BASE + 10)
#define TCP_BWEMU_SET_ALL       (TCP_BWEMU_OPT_BASE + 11)

#endif

typedef struct {
  int modem_id;
  int fw_drop_rate;   // forword percentege loss
  int rv_drop_rate;   // reverse percentege loss
  int fw_fixed_delay; // forword fixed delay 
  int rv_fixed_delay; // reverse fixed delay
  int fw_compress;    // Tx compression
  int rv_compress;    // Rx compression
  int fw_jitter;      // +- millisec (10 = +- 10MS)
  int rv_jitter;      // +- millisec (10 = +- 10MS)
} bwemu_modem_info;

struct cav_params {
	int conn_speed;			/* bps */
};

struct cav_class_params {
	int modem_id;
	struct cav_params params[2];
        int compression;
};

#define NAME_LENGTH 128

#define CAV_SET_CLASS		_IOW('C', 1, struct cav_class_params)
#define CAV_GET_CLASS		_IOWR('C', 2, struct cav_class_params)
#define CAV_OPEN_MODEM		_IOR('C', 3, int)
#define CAV_OPEN_SHARED_MODEM   _IOWR('C', 4, int)
#define CAV_CLOSE_MODEM		_IOW('C', 5, int)
#define CAV_CLOSE_ALL_MODEMS	_IO('C', 6)
#define CAV_GET_STATS		_IO('C', 7)
#define CAV_SET_STATS		_IO('C', 8)

#endif

