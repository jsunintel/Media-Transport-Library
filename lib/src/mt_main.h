/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#define _GNU_SOURCE
#include <math.h>
#include <rte_alarm.h>
#include <rte_arp.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#ifdef MTL_HAS_KNI
#include <rte_kni.h>
#endif
#include <json-c/json.h>
#include <rte_tm.h>
#include <rte_version.h>
#include <rte_vfio.h>
#include <sys/file.h>
#include <unistd.h>

#include "mt_mem.h"
#include "mt_platform.h"
#include "mt_queue.h"
#include "mt_quirk.h"
#include "st2110/st_header.h"

#ifndef _MT_LIB_MAIN_HEAD_H_
#define _MT_LIB_MAIN_HEAD_H_

#define MT_MAY_UNUSED(x) (void)(x)

#define MT_MBUF_CACHE_SIZE (128)
#define MT_MBUF_HEADROOM_SIZE (RTE_PKTMBUF_HEADROOM)          /* 128 */
#define MT_MBUF_DEFAULT_DATA_SIZE (RTE_MBUF_DEFAULT_DATAROOM) /* 2048 */

#define MT_MAX_SCH_NUM (18)         /* max 18 scheduler lcore */
#define MT_MAX_TASKLET_PER_SCH (16) /* max 16 tasklet in one scheduler lcore */

/* max RL items */
#define MT_MAX_RL_ITEMS (64)

#define MT_ARP_ENTRY_MAX (60)

#define MT_MCAST_GROUP_MAX (60)

#define MT_DMA_MAX_SESSIONS (16)
/* if use rte ring for dma enqueue/dequeue */
#define MT_DMA_RTE_RING (1)

#define MT_MAP_MAX_ITEMS (256)

#define MT_IP_DONT_FRAGMENT_FLAG (0x0040)

/* Port supports Rx queue setup after device started. */
#define MT_IF_FEATURE_RUNTIME_RX_QUEUE (MTL_BIT32(0))
/* Timesync enabled on the port */
#define MT_IF_FEATURE_TIMESYNC (MTL_BIT32(1))
/* Port registers Rx timestamp in mbuf dynamic field */
#define MT_IF_FEATURE_RX_OFFLOAD_TIMESTAMP (MTL_BIT32(2))
/* Multi segment tx, chain buffer */
#define MT_IF_FEATURE_TX_MULTI_SEGS (MTL_BIT32(4))
/* tx ip hdr checksum offload */
#define MT_IF_FEATURE_TX_OFFLOAD_IPV4_CKSUM (MTL_BIT32(5))
/* Rx queue support hdr split */
#define MT_IF_FEATURE_RXQ_OFFLOAD_BUFFER_SPLIT (MTL_BIT32(6))

#define MT_IF_STAT_PORT_CONFIGED (MTL_BIT32(0))
#define MT_IF_STAT_PORT_STARTED (MTL_BIT32(1))

#define NS_PER_MS (1000 * 1000)
#define NS_PER_US (1000)
#define US_PER_MS (1000)

struct mtl_main_impl; /* foward declare */

/* dynamic fields are implemented after rte_mbuf */
struct mt_muf_priv_data {
  union {
    struct st_tx_muf_priv_data tx_priv;
    struct st_rx_muf_priv_data rx_priv;
  };
};

struct mt_ptp_clock_id {
  uint8_t id[8];
};

struct mt_ptp_port_id {
  struct mt_ptp_clock_id clock_identity;
  uint16_t port_number;
} __attribute__((packed));

struct mt_ptp_ipv4_udp {
  struct rte_ipv4_hdr ip;
  struct rte_udp_hdr udp;
} __attribute__((__packed__));

enum mt_port_type {
  MT_PORT_ERR = 0,
  MT_PORT_VF,
  MT_PORT_PF,
  MT_PORT_AF_XDP,
};

enum mt_driver_type {
  MT_DRV_ERR = 0,
  MT_DRV_ICE,       /* ice pf, net_ice */
  MT_DRV_I40E,      /* flv pf, net_i40e */
  MT_DRV_IAVF,      /* IA vf, net_iavf */
  MT_DRV_AF_XDP,    /* af xdp, net_af_xdp */
  MT_DRV_E1000_IGB, /* e1000 igb, net_e1000_igb */
  MT_DRV_IGC,       /* igc, net_igc */
};

enum mt_ptp_l_mode {
  MT_PTP_L2 = 0,
  MT_PTP_L4,
  MT_PTP_MAX_MODE,
};

enum mt_ptp_addr_mode {
  MT_PTP_MULTICAST_ADDR = 0,
  MT_PTP_UNICAST_ADDR,
};

struct mt_ptp_impl {
  struct mtl_main_impl* impl;
  enum mtl_port port;
  uint16_t port_id;

  struct mt_rx_queue* rx_queue;
  struct rte_mempool* mbuf_pool;

  uint8_t mcast_group_addr[MTL_IP_ADDR_LEN]; /* 224.0.1.129 */
  bool master_initialized;
  struct mt_ptp_port_id master_port_id;
  struct rte_ether_addr master_addr;
  struct mt_ptp_port_id our_port_id;
  struct mt_ptp_ipv4_udp dst_udp;    /* for l4 */
  uint8_t sip_addr[MTL_IP_ADDR_LEN]; /* source IP */
  enum mt_ptp_addr_mode master_addr_mode;
  int16_t master_utc_offset; /* offset to UTC of current master PTP */
  int64_t ptp_delta;         /* current delta for PTP */

  uint64_t t1;
  uint8_t t1_domain_number;
  uint64_t t2;
  bool t2_vlan;
  uint16_t t2_sequence_id;
  enum mt_ptp_l_mode t2_mode;
  uint64_t t3;
  uint16_t t3_sequence_id;
  uint64_t t4;
  /* result */
  uint64_t delta_result_cnt;
  uint64_t delta_result_sum;
  uint64_t delta_result_err;
  /* expect result */
  int32_t expect_result_cnt;
  int32_t expect_result_sum;
  int32_t expect_result_avg;
  uint64_t expect_result_start_ns;
  uint64_t expect_result_period_ns;

  /* calculate sw frequency */
  uint64_t last_sync_ts;
  double coefficient;
  double coefficient_result_sum;
  double coefficient_result_min;
  double coefficient_result_max;
  int32_t coefficient_result_cnt;

  /* status */
  int64_t stat_delta_min;
  int64_t stat_delta_max;
  int32_t stat_delta_cnt;
  int64_t stat_delta_sum;
  int64_t stat_correct_delta_min;
  int64_t stat_correct_delta_max;
  int32_t stat_correct_delta_cnt;
  int64_t stat_correct_delta_sum;
  int64_t stat_path_delay_min;
  int64_t stat_path_delay_max;
  int32_t stat_path_delay_cnt;
  int64_t stat_path_delay_sum;
  int32_t stat_rx_sync_err;
  int32_t stat_tx_sync_err;
  int32_t stat_result_err;
  int32_t stat_sync_timeout_err;
  int32_t stat_sync_cnt;
};

struct mt_cni_impl {
  bool used; /* if enable cni */

  struct mt_rx_queue* rx_q[MTL_PORT_MAX]; /* cni rx queue */
  pthread_t tid;                          /* thread id for rx */
  rte_atomic32_t stop_thread;
  bool lcore_tasklet;
  struct mt_sch_tasklet_impl* tasklet;
  /* stat */
  int eth_rx_cnt[MTL_PORT_MAX];

#ifdef MTL_HAS_KNI
  bool has_kni_kmod;
  rte_atomic32_t if_up[MTL_PORT_MAX];
  struct rte_kni_conf conf[MTL_PORT_MAX];
  struct rte_kni* rkni[MTL_PORT_MAX];
  pthread_t kni_bkg_tid; /* bkg thread id for kni */
  rte_atomic32_t stop_kni;
  int kni_rx_cnt[MTL_PORT_MAX];
#endif

#ifdef MTL_HAS_TAP
  pthread_t tap_bkg_tid; /* bkg thread id for tap */
  rte_atomic32_t stop_tap;
  struct mt_tx_queue* tap_tx_q[MTL_PORT_MAX]; /* tap tx queue */
  struct mt_rx_queue* tap_rx_q[MTL_PORT_MAX]; /* tap rx queue */
  int tap_rx_cnt[MTL_PORT_MAX];
  rte_atomic32_t tap_if_up[MTL_PORT_MAX];
  void* tap_context;
#endif
};

struct mt_arp_impl {
  pthread_mutex_t mutex; /* entry protect */
  uint32_t ip[MT_ARP_ENTRY_MAX];
  struct rte_ether_addr ea[MT_ARP_ENTRY_MAX];
  rte_atomic32_t mac_ready[MT_ARP_ENTRY_MAX];
};

struct mt_mcast_impl {
  pthread_mutex_t group_mutex;
  uint32_t group_ip[MT_MCAST_GROUP_MAX];
  uint32_t group_ref_cnt[MT_MCAST_GROUP_MAX];
  uint16_t group_num;
};

#define MT_TASKLET_HAS_PENDING (1)
#define MT_TASKLET_ALL_DONE (0)

/*
 * Tasklets share the time slot on a lcore,
 * only non-block method can be used in handler routine.
 */
struct mt_sch_tasklet_ops {
  char* name;
  void* priv; /* private data to the callback */

  int (*pre_start)(void* priv);
  int (*start)(void* priv);
  int (*stop)(void* priv);
  /*
   * return MT_TASKLET_ALL_DONE if no any pending task, all are done
   * return MT_TASKLET_HAS_PENDING if it has pending tasks.
   */
  int (*handler)(void* priv);
  /*
   * the recommend sleep time(us) if tasklet report MT_TASKLET_ALL_DONE.
   * also this value can be set by mt_tasklet_set_sleep at runtime.
   * leave to zero if you don't know.
   */
  uint64_t advice_sleep_us;
};

struct mt_sch_tasklet_impl {
  struct mt_sch_tasklet_ops ops;
  char name[ST_MAX_NAME_LEN];
  struct mt_sch_impl* sch;

  int idx;

  uint32_t stat_max_time_us;
  uint64_t stat_sum_time_us;
  uint64_t stat_time_cnt;
  uint32_t stat_min_time_us;
};

enum mt_sch_type {
  MT_SCH_TYPE_DEFAULT = 0,
  MT_SCH_TYPE_RX_VIDEO_ONLY,
  MT_SCH_TYPE_MAX,
};

typedef uint64_t mt_sch_mask_t;

/* all sch */
#define MT_SCH_MASK_ALL ((mt_sch_mask_t)-1)

struct mt_sch_impl {
  pthread_mutex_t mutex; /* protect sch context */
  struct mt_sch_tasklet_impl* tasklet[MT_MAX_TASKLET_PER_SCH];
  int max_tasklet_idx; /* max tasklet index */
  unsigned int lcore;
  bool run_in_thread; /* Run the tasklet inside one thread instead of a pinned lcore. */
  pthread_t tid;      /* thread id for run_in_thread */

  int data_quota_mbs_total; /* total data quota(mb/s) for current sch */
  int data_quota_mbs_limit; /* limit data quota(mb/s) for current sch */
  bool cpu_busy;

  struct mtl_main_impl* parnet;
  int idx; /* index for current sch */
  rte_atomic32_t started;
  rte_atomic32_t request_stop;
  rte_atomic32_t stopped;
  rte_atomic32_t active; /* if this sch is active */
  rte_atomic32_t ref_cnt;
  enum mt_sch_type type;

  /* one tx video sessions mgr/transmitter for one sch */
  struct st_video_transmitter_impl video_transmitter;
  struct st_tx_video_sessions_mgr tx_video_mgr;
  bool tx_video_init;
  pthread_mutex_t tx_video_mgr_mutex; /* protect tx_video_mgr */

  /* one rx video sessions mgr for one sch */
  struct st_rx_video_sessions_mgr rx_video_mgr;
  bool rx_video_init;
  pthread_mutex_t rx_video_mgr_mutex; /* protect tx_video_mgr */

  /* sch sleep info */
  bool allow_sleep;
  pthread_cond_t sleep_wake_cond;
  pthread_mutex_t sleep_wake_mutex;

  /* the sch sleep ratio */
  float sleep_ratio_score;
  uint64_t sleep_ratio_start_ns;
  uint64_t sleep_ratio_sleep_ns;

  uint64_t stat_sleep_ns;
  uint32_t stat_sleep_cnt;
  uint64_t stat_sleep_ns_min;
  uint64_t stat_sleep_ns_max;
};

struct mt_sch_mgr {
  struct mt_sch_impl sch[MT_MAX_SCH_NUM];
  /* active sch cnt */
  rte_atomic32_t sch_cnt;
  pthread_mutex_t mgr_mutex; /* protect sch mgr */
};

struct mt_pacing_train_result {
  uint64_t rl_bps;           /* input, byte per sec */
  float pacing_pad_interval; /* result */
};

struct mt_rl_shaper {
  uint64_t rl_bps; /* input, byte per sec */
  uint32_t shaper_profile_id;
  int idx;
};

struct mt_rx_flow {
  uint8_t dip_addr[MTL_IP_ADDR_LEN]; /* rx destination IP */
  uint8_t sip_addr[MTL_IP_ADDR_LEN]; /* source IP */
  uint16_t dst_port;                 /* udp destination port */
  int flow_id;                       /* flow id in the eth tool */
  bool port_flow;                    /* if apply port flow */
  bool hdr_split;                    /* if request hdr split */
  void* hdr_split_mbuf_cb_priv;
#ifdef ST_HAS_DPDK_HDR_SPLIT /* rte_eth_hdrs_mbuf_callback_fn define with this marco */
  rte_eth_hdrs_mbuf_callback_fn hdr_split_mbuf_cb;
#endif
};

struct mt_rx_queue {
  enum mtl_port port;
  uint16_t port_id;
  uint16_t queue_id;
  bool active;
  struct rte_flow* flow;
  struct mt_rx_flow st_flow;
  struct rte_mempool* mbuf_pool;
  unsigned int mbuf_elements;
  /* pool for hdr split payload */
  struct rte_mempool* mbuf_payload_pool;
};

struct mt_tx_queue {
  enum mtl_port port;
  uint16_t port_id;
  uint16_t queue_id;
  bool active;
  int rl_shapers_mapping; /* map to tx_rl_shapers */
  uint64_t bps;           /* bytes per sec for rate limit */
};

struct mt_interface {
  enum mtl_port port;
  uint16_t port_id;
  struct rte_device* device;
  enum mt_port_type port_type;
  enum mt_driver_type drv_type;
  int socket_id;                          /* socket id for the port */
  uint32_t feature;                       /* MT_IF_FEATURE_* */
  uint32_t link_speed;                    /* ETH_SPEED_NUM_ */
  struct rte_ether_addr* mcast_mac_lists; /* pool of multicast mac addrs */
  uint32_t mcast_nb;                      /* number of address */
  uint32_t status;                        /* MT_IF_STAT_* */

  /* default tx mbuf_pool */
  struct rte_mempool* tx_mbuf_pool;
  /* default rx mbuf_pool */
  struct rte_mempool* rx_mbuf_pool;
  uint16_t nb_tx_desc;
  uint16_t nb_rx_desc;

  struct rte_mbuf* pad;

  /* tx queue resources */
  int max_tx_queues;
  struct mt_tx_queue* tx_queues;
  pthread_mutex_t tx_queues_mutex; /* protect tx_queues */

  /* the shared tx sys queue */
  struct mt_tx_queue* tx_sys_queue;
  pthread_mutex_t tx_sys_queue_mutex; /* protect tx_sys_queue */

  /* rx queue resources */
  int max_rx_queues;
  int system_rx_queues_end;
  int hdr_split_rx_queues_end;
  struct mt_rx_queue* rx_queues;
  pthread_mutex_t rx_queues_mutex; /* protect rx_queues */

  /* tx rl info */
  struct mt_rl_shaper tx_rl_shapers[MT_MAX_RL_ITEMS];
  bool tx_rl_root_active;
  /* video rl pacing train result */
  struct mt_pacing_train_result pt_results[MT_MAX_RL_ITEMS];

  /* function ops per interface(pf/vf) */
  uint64_t (*ptp_get_time_fn)(struct mtl_main_impl* impl, enum mtl_port port);

  enum st21_tx_pacing_way tx_pacing_way;
};

struct mt_lcore_shm {
  int used;                          /* number of used lcores */
  bool lcores_active[RTE_MAX_LCORE]; /* lcores active map */
};

typedef int (*mt_dma_drop_mbuf_cb)(void* priv, struct rte_mbuf* mbuf);

struct mtl_dma_lender_dev {
  enum mt_handle_type type; /* for sanity check */

  struct mt_dma_dev* parent;
  int lender_id;
  bool active;

  void* priv;
  uint16_t nb_borrowed;
  mt_dma_drop_mbuf_cb cb;
};

struct mt_dma_dev {
  int16_t dev_id;
  uint16_t nb_desc;
  bool active;
  bool usable;
  int idx;
  int sch_idx;
  int soc_id;
  uint16_t nb_session; /* number of attached session(lender)s */
  uint16_t max_shared; /* max number of attached session(lender)s */
  /* shared lenders */
  struct mtl_dma_lender_dev lenders[MT_DMA_MAX_SESSIONS];
  uint16_t nb_inflight; /* not atomic since it's in single thread only */
#if MT_DMA_RTE_RING
  struct rte_ring* borrow_queue; /* borrowed mbufs from rx sessions */
#else
  uint16_t inflight_enqueue_idx;
  uint16_t inflight_dequeue_idx;
  struct rte_mbuf** inflight_mbufs;
#endif
  uint64_t stat_inflight_sum;
  uint64_t stat_commit_sum;
};

struct mt_dma_mgr {
  struct mt_dma_dev devs[MTL_DMA_DEV_MAX];
  pthread_mutex_t mutex; /* protect devs */
  uint8_t num_dma_dev;
  rte_atomic32_t num_dma_dev_active;
};

struct mtl_dma_mem {
  void* alloc_addr;  /* the address return from malloc */
  size_t alloc_size; /* the malloc size */
  void* addr;        /* the first page aligned address after alloc_addr */
  size_t valid_size; /* the valid data size from user */
  mtl_iova_t iova;   /* the dma mapped address of addr */
  size_t iova_size;  /* the iova mapped size */
};

struct mt_admin {
  uint64_t period_us;
  pthread_t admin_tid;
  pthread_cond_t admin_wake_cond;
  pthread_mutex_t admin_wake_mutex;
  rte_atomic32_t admin_stop;
};

struct mt_kport_info {
  /* dpdk port name for kernel port */
  char port[MTL_PORT_MAX][MTL_PORT_MAX_LEN];
};

struct mt_map_item {
  void* vaddr;
  size_t size;
  mtl_iova_t iova; /* iova address */
};

struct mt_map_mgr {
  pthread_mutex_t mutex;
  struct mt_map_item* items[MT_MAP_MAX_ITEMS];
};

struct mt_var_params {
  /* default sleep time(us) for sch tasklet sleep */
  uint64_t sch_default_sleep_us;
  /* force sleep time(us) for sch tasklet sleep */
  uint64_t sch_force_sleep_us;
  /* sleep(0) threshold */
  uint64_t sch_zero_sleep_threshold_us;
};

typedef int (*mt_stat_cb_t)(void* priv);
struct mt_stat_item {
  /* stat dump callback func */
  mt_stat_cb_t cb_func;
  /* stat dump callback private data */
  void* cb_priv;
  /* linked list */
  MT_TAILQ_ENTRY(mt_stat_item) next;
};
/* List of stat items */
MT_TAILQ_HEAD(mt_stat_items_list, mt_stat_item);

struct mt_stat_mgr {
  pthread_mutex_t mutex;
  struct mt_stat_items_list head;
};

struct mtl_main_impl {
  struct mt_interface inf[MTL_PORT_MAX];

  struct mtl_init_params user_para;
  struct mt_var_params var_para;
  struct mt_kport_info kport_info;
  enum mt_handle_type type; /* for sanity check */
  uint64_t tsc_hz;
  pthread_t tsc_cal_tid;

  enum rte_iova_mode iova_mode;
  size_t page_size;

  /* stat */
  pthread_t stat_tid;
  pthread_cond_t stat_wake_cond;
  pthread_mutex_t stat_wake_mutex;
  rte_atomic32_t stat_stop;
  struct mt_stat_mgr stat_mgr;

  /* dev context */
  rte_atomic32_t instance_started;  /* if mt instance is started */
  rte_atomic32_t instance_in_reset; /* if mt instance is in reset */
  /* if mt instance is aborted, in case for ctrl-c from app */
  rte_atomic32_t instance_aborted;
  struct mt_sch_impl* main_sch; /* system sch */

  /* admin context */
  struct mt_admin admin;

  /* cni context */
  struct mt_cni_impl cni;

  /* ptp context */
  struct mt_ptp_impl ptp[MTL_PORT_MAX];

  /* arp context */
  struct mt_arp_impl arp[MTL_PORT_MAX];

  /* mcast context */
  struct mt_mcast_impl mcast[MTL_PORT_MAX];

  /* sch context */
  struct mt_sch_mgr sch_mgr;

  /* st plugin dev mgr */
  struct st_plugin_mgr plugin_mgr;

  /* audio(st_30) context */
  struct st_tx_audio_sessions_mgr tx_a_mgr;
  struct st_audio_transmitter_impl a_trs;
  struct st_rx_audio_sessions_mgr rx_a_mgr;
  bool tx_a_init;
  pthread_mutex_t tx_a_mgr_mutex; /* protect tx_a_mgr */
  bool rx_a_init;
  pthread_mutex_t rx_a_mgr_mutex; /* protect rx_a_mgr */

  /* ancillary(st_40) context */
  struct st_tx_ancillary_sessions_mgr tx_anc_mgr;
  struct st_ancillary_transmitter_impl anc_trs;
  struct st_rx_ancillary_sessions_mgr rx_anc_mgr;
  bool tx_anc_init;
  pthread_mutex_t tx_anc_mgr_mutex; /* protect tx_anc_mgr */
  bool rx_anc_init;
  pthread_mutex_t rx_anc_mgr_mutex; /* protect rx_anc_mgr */

  /* max sessions supported */
  uint16_t tx_sessions_cnt_max;
  uint16_t rx_sessions_cnt_max;
  /* cnt for open sessions */
  rte_atomic32_t st20_tx_sessions_cnt;
  rte_atomic32_t st22_tx_sessions_cnt;
  rte_atomic32_t st30_tx_sessions_cnt;
  rte_atomic32_t st40_tx_sessions_cnt;
  rte_atomic32_t st20_rx_sessions_cnt;
  rte_atomic32_t st22_rx_sessions_cnt;
  rte_atomic32_t st30_rx_sessions_cnt;
  rte_atomic32_t st40_rx_sessions_cnt;
  /* active lcore cnt */
  rte_atomic32_t lcore_cnt;

  struct mt_lcore_shm* lcore_shm;
  int lcore_shm_id;
  int lcore_lock_fd;
  bool local_lcores_active[RTE_MAX_LCORE]; /* local lcores active map */

  /* rx timestamp register */
  int dynfield_offset;

  struct mt_dma_mgr dma_mgr;

  struct mt_map_mgr map_mgr;

  uint16_t pkt_udp_suggest_max_size;
  uint16_t rx_pool_data_size;
  uint32_t sch_schedule_ns;
};

static inline struct mtl_init_params* mt_get_user_params(struct mtl_main_impl* impl) {
  return &impl->user_para;
}

static inline struct mt_interface* mt_if(struct mtl_main_impl* impl, enum mtl_port port) {
  return &impl->inf[port];
}

static inline uint16_t mt_port_id(struct mtl_main_impl* impl, enum mtl_port port) {
  return mt_if(impl, port)->port_id;
}

static inline struct rte_device* mt_port_device(struct mtl_main_impl* impl,
                                                enum mtl_port port) {
  return mt_if(impl, port)->device;
}

static inline enum mt_port_type mt_port_type(struct mtl_main_impl* impl,
                                             enum mtl_port port) {
  return mt_if(impl, port)->port_type;
}

enum mtl_port mt_port_by_id(struct mtl_main_impl* impl, uint16_t port_id);

static inline uint8_t* mt_sip_addr(struct mtl_main_impl* impl, enum mtl_port port) {
  return mt_get_user_params(impl)->sip_addr[port];
}

static inline enum mtl_pmd_type mt_pmd_type(struct mtl_main_impl* impl,
                                            enum mtl_port port) {
  return mt_get_user_params(impl)->pmd[port];
}

static inline bool mt_pmd_is_kernel(struct mtl_main_impl* impl, enum mtl_port port) {
  if (MTL_PMD_DPDK_USER == mt_get_user_params(impl)->pmd[port])
    return false;
  else
    return true;
}

static inline bool mt_pmd_is_af_xdp(struct mtl_main_impl* impl, enum mtl_port port) {
  if (MTL_PMD_DPDK_AF_XDP == mt_get_user_params(impl)->pmd[port])
    return true;
  else
    return false;
}

static inline int mt_num_ports(struct mtl_main_impl* impl) {
  return RTE_MIN(mt_get_user_params(impl)->num_ports, MTL_PORT_MAX);
}

bool mt_is_valid_socket(struct mtl_main_impl* impl, int soc_id);

static inline uint8_t mt_start_queue(struct mtl_main_impl* impl, enum mtl_port port) {
  return impl->user_para.xdp_info[port].start_queue;
}

static inline bool mt_has_ptp_service(struct mtl_main_impl* impl) {
  if (mt_get_user_params(impl)->flags & MTL_FLAG_PTP_ENABLE)
    return true;
  else
    return false;
}

static inline bool mt_has_auto_start_stop(struct mtl_main_impl* impl) {
  if (mt_get_user_params(impl)->flags & MTL_FLAG_DEV_AUTO_START_STOP)
    return true;
  else
    return false;
}

static inline bool mt_has_af_xdp_zc(struct mtl_main_impl* impl) {
  if (mt_get_user_params(impl)->flags & MTL_FLAG_AF_XDP_ZC_DISABLE)
    return false;
  else
    return true;
}

static inline bool mt_has_user_ptp(struct mtl_main_impl* impl) {
  if (mt_get_user_params(impl)->ptp_get_time_fn)
    return true;
  else
    return false;
}

static inline bool mt_has_user_quota(struct mtl_main_impl* impl) {
  if (mt_get_user_params(impl)->data_quota_mbs_per_sch)
    return true;
  else
    return false;
}

static inline bool mt_has_ebu(struct mtl_main_impl* impl) {
  if (mt_get_user_params(impl)->flags & MTL_FLAG_RX_VIDEO_EBU)
    return true;
  else
    return false;
}

static inline bool mt_has_rxv_separate_sch(struct mtl_main_impl* impl) {
  if (mt_get_user_params(impl)->flags & MTL_FLAG_RX_SEPARATE_VIDEO_LCORE)
    return true;
  else
    return false;
}

static inline bool mt_has_tx_video_migrate(struct mtl_main_impl* impl) {
  if (mt_get_user_params(impl)->flags & MTL_FLAG_TX_VIDEO_MIGRATE)
    return true;
  else
    return false;
}

static inline bool mt_has_rx_video_migrate(struct mtl_main_impl* impl) {
  if (mt_get_user_params(impl)->flags & MTL_FLAG_RX_VIDEO_MIGRATE)
    return true;
  else
    return false;
}

static inline bool mt_has_tasklet_time_measure(struct mtl_main_impl* impl) {
  if (mt_get_user_params(impl)->flags & MTL_FLAG_TASKLET_TIME_MEASURE)
    return true;
  else
    return false;
}

static inline bool mt_has_rx_mono_pool(struct mtl_main_impl* impl) {
  if (mt_get_user_params(impl)->flags & MTL_FLAG_RX_MONO_POOL)
    return true;
  else
    return false;
}

static inline bool mt_has_tx_mono_pool(struct mtl_main_impl* impl) {
  if (mt_get_user_params(impl)->flags & MTL_FLAG_TX_MONO_POOL)
    return true;
  else
    return false;
}

static inline bool mt_no_system_rxq(struct mtl_main_impl* impl) {
  if (mt_get_user_params(impl)->flags & MTL_FLAG_DISABLE_SYSTEM_RX_QUEUES)
    return true;
  else
    return false;
}

static inline bool mt_tasklet_has_thread(struct mtl_main_impl* impl) {
  if (mt_get_user_params(impl)->flags & MTL_FLAG_TASKLET_THREAD)
    return true;
  else
    return false;
}

static inline bool mt_tasklet_has_sleep(struct mtl_main_impl* impl) {
  if (mt_get_user_params(impl)->flags & MTL_FLAG_TASKLET_SLEEP)
    return true;
  else
    return false;
}

static inline bool mt_if_has_timesync(struct mtl_main_impl* impl, enum mtl_port port) {
  if (mt_if(impl, port)->feature & MT_IF_FEATURE_TIMESYNC)
    return true;
  else
    return false;
}

static inline bool mt_if_has_offload_ipv4_cksum(struct mtl_main_impl* impl,
                                                enum mtl_port port) {
  if (mt_if(impl, port)->feature & MT_IF_FEATURE_TX_OFFLOAD_IPV4_CKSUM)
    return true;
  else
    return false;
}

static inline bool mt_if_has_chain_buff(struct mtl_main_impl* impl, enum mtl_port port) {
  if (mt_if(impl, port)->feature & MT_IF_FEATURE_TX_MULTI_SEGS)
    return true;
  else
    return false;
}

static inline bool mt_if_has_hdr_split(struct mtl_main_impl* impl, enum mtl_port port) {
  if (mt_if(impl, port)->feature & MT_IF_FEATURE_RXQ_OFFLOAD_BUFFER_SPLIT)
    return true;
  else
    return false;
}

static inline struct rte_mempool* mt_if_hdr_split_pool(struct mt_interface* inf,
                                                       uint16_t q) {
  return inf->rx_queues[q].mbuf_payload_pool;
}

static inline bool mt_if_has_ptp(struct mtl_main_impl* impl, enum mtl_port port) {
  if (mt_has_ptp_service(impl) && mt_if_has_timesync(impl, port))
    return true;
  else
    return false;
}

static inline uint16_t mt_if_nb_tx_desc(struct mtl_main_impl* impl, enum mtl_port port) {
  return mt_if(impl, port)->nb_tx_desc;
}

static inline uint16_t mt_if_nb_rx_desc(struct mtl_main_impl* impl, enum mtl_port port) {
  return mt_if(impl, port)->nb_rx_desc;
}

static inline int mt_socket_id(struct mtl_main_impl* impl, enum mtl_port port) {
  return mt_if(impl, port)->socket_id;
}

static inline bool mt_started(struct mtl_main_impl* impl) {
  if (rte_atomic32_read(&impl->instance_started))
    return true;
  else
    return false;
}

static inline bool mt_in_reset(struct mtl_main_impl* impl) {
  if (rte_atomic32_read(&impl->instance_in_reset))
    return true;
  else
    return false;
}

static inline bool mt_aborted(struct mtl_main_impl* impl) {
  if (rte_atomic32_read(&impl->instance_aborted))
    return true;
  else
    return false;
}

static inline uint32_t mt_sch_schedule_ns(struct mtl_main_impl* impl) {
  return impl->sch_schedule_ns;
}

static inline struct rte_mempool* mt_get_tx_mempool(struct mtl_main_impl* impl,
                                                    enum mtl_port port) {
  return mt_if(impl, port)->tx_mbuf_pool;
}

static inline struct rte_mempool* mt_get_rx_mempool(struct mtl_main_impl* impl,
                                                    enum mtl_port port) {
  return mt_if(impl, port)->rx_mbuf_pool;
}

static inline struct rte_mbuf* mt_get_pad(struct mtl_main_impl* impl,
                                          enum mtl_port port) {
  return mt_if(impl, port)->pad;
}

static inline struct mt_dma_mgr* mt_get_dma_mgr(struct mtl_main_impl* impl) {
  return &impl->dma_mgr;
}

static inline uint64_t mt_sch_default_sleep_us(struct mtl_main_impl* impl) {
  return impl->var_para.sch_default_sleep_us;
}

static inline uint64_t mt_sch_force_sleep_us(struct mtl_main_impl* impl) {
  return impl->var_para.sch_force_sleep_us;
}

static inline uint64_t mt_sch_zero_sleep_thresh_us(struct mtl_main_impl* impl) {
  return impl->var_para.sch_zero_sleep_threshold_us;
}

static inline void mt_sleep_ms(unsigned int ms) { return rte_delay_us_sleep(ms * 1000); }

static inline void mt_delay_us(unsigned int us) { return rte_delay_us_block(us); }

static inline void mt_free_mbufs(struct rte_mbuf** pkts, int num) {
  for (int i = 0; i < num; i++) {
    rte_pktmbuf_free(pkts[i]);
    pkts[i] = NULL;
  }
}

static inline void mt_mbuf_init_ipv4(struct rte_mbuf* pkt) {
  pkt->l2_len = sizeof(struct rte_ether_hdr); /* 14 */
  pkt->l3_len = sizeof(struct rte_ipv4_hdr);  /* 20 */
#if RTE_VERSION >= RTE_VERSION_NUM(21, 11, 0, 0)
  pkt->ol_flags |= RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_IP_CKSUM;
#else
  pkt->ol_flags |= PKT_TX_IPV4 | PKT_TX_IP_CKSUM;
#endif
}

static inline uint64_t mt_timespec_to_ns(const struct timespec* ts) {
  return ((uint64_t)ts->tv_sec * NS_PER_S) + ts->tv_nsec;
}

static inline void mt_ns_to_timespec(uint64_t ns, struct timespec* ts) {
  ts->tv_sec = ns / NS_PER_S;
  ts->tv_nsec = ns % NS_PER_S;
}

static inline int mt_wait_tsc_stable(struct mtl_main_impl* impl) {
  if (impl->tsc_cal_tid) {
    pthread_join(impl->tsc_cal_tid, NULL);
    impl->tsc_cal_tid = 0;
  }

  return 0;
}

/* Return relative TSC time in nanoseconds */
static inline uint64_t mt_get_tsc(struct mtl_main_impl* impl) {
  double tsc = rte_get_tsc_cycles();
  double tsc_hz = impl->tsc_hz;
  double time_nano = tsc / (tsc_hz / ((double)NS_PER_S));
  return time_nano;
}

/* busy loop until target time reach */
static inline void mt_tsc_delay_to(struct mtl_main_impl* impl, uint64_t target) {
  while (mt_get_tsc(impl) < target) {
  }
}

/* Monotonic time (in nanoseconds) since some unspecified starting point. */
static inline uint64_t mt_get_monotonic_time() {
  struct timespec ts;

  clock_gettime(MT_CLOCK_MONOTONIC_ID, &ts);
  return ((uint64_t)ts.tv_sec * NS_PER_S) + ts.tv_nsec;
}

static inline void st_tx_mbuf_set_tsc(struct rte_mbuf* mbuf, uint64_t time_stamp) {
  struct mt_muf_priv_data* priv = rte_mbuf_to_priv(mbuf);
  priv->tx_priv.tsc_time_stamp = time_stamp;
}

static inline uint64_t st_tx_mbuf_get_tsc(struct rte_mbuf* mbuf) {
  struct mt_muf_priv_data* priv = rte_mbuf_to_priv(mbuf);
  return priv->tx_priv.tsc_time_stamp;
}

static inline void st_tx_mbuf_set_ptp(struct rte_mbuf* mbuf, uint64_t time_stamp) {
  struct mt_muf_priv_data* priv = rte_mbuf_to_priv(mbuf);
  priv->tx_priv.ptp_time_stamp = time_stamp;
}

static inline uint64_t st_tx_mbuf_get_ptp(struct rte_mbuf* mbuf) {
  struct mt_muf_priv_data* priv = rte_mbuf_to_priv(mbuf);
  return priv->tx_priv.ptp_time_stamp;
}

static inline void st_tx_mbuf_set_idx(struct rte_mbuf* mbuf, uint32_t idx) {
  struct mt_muf_priv_data* priv = rte_mbuf_to_priv(mbuf);
  priv->tx_priv.idx = idx;
}

static inline uint32_t st_tx_mbuf_get_idx(struct rte_mbuf* mbuf) {
  struct mt_muf_priv_data* priv = rte_mbuf_to_priv(mbuf);
  return priv->tx_priv.idx;
}

static inline void st_rx_mbuf_set_lender(struct rte_mbuf* mbuf, uint32_t lender) {
  struct mt_muf_priv_data* priv = rte_mbuf_to_priv(mbuf);
  priv->rx_priv.lender = lender;
}

static inline uint32_t st_rx_mbuf_get_lender(struct rte_mbuf* mbuf) {
  struct mt_muf_priv_data* priv = rte_mbuf_to_priv(mbuf);
  return priv->rx_priv.lender;
}

static inline void st_rx_mbuf_set_offset(struct rte_mbuf* mbuf, uint32_t offset) {
  struct mt_muf_priv_data* priv = rte_mbuf_to_priv(mbuf);
  priv->rx_priv.offset = offset;
}

static inline uint32_t st_rx_mbuf_get_offset(struct rte_mbuf* mbuf) {
  struct mt_muf_priv_data* priv = rte_mbuf_to_priv(mbuf);
  return priv->rx_priv.offset;
}

static inline void st_rx_mbuf_set_len(struct rte_mbuf* mbuf, uint32_t len) {
  struct mt_muf_priv_data* priv = rte_mbuf_to_priv(mbuf);
  priv->rx_priv.len = len;
}

static inline uint32_t st_rx_mbuf_get_len(struct rte_mbuf* mbuf) {
  struct mt_muf_priv_data* priv = rte_mbuf_to_priv(mbuf);
  return priv->rx_priv.len;
}

uint64_t mt_mbuf_hw_time_stamp(struct mtl_main_impl* impl, struct rte_mbuf* mbuf);

static inline uint64_t mt_get_ptp_time(struct mtl_main_impl* impl, enum mtl_port port) {
  return mt_if(impl, port)->ptp_get_time_fn(impl, port);
}

uint64_t mt_get_raw_ptp_time(struct mtl_main_impl* impl, enum mtl_port port);

#if RTE_VERSION >= RTE_VERSION_NUM(21, 11, 0, 0)
static inline struct rte_ether_addr* mt_eth_s_addr(struct rte_ether_hdr* eth) {
  return &eth->src_addr;
}

static inline struct rte_ether_addr* mt_eth_d_addr(struct rte_ether_hdr* eth) {
  return &eth->dst_addr;
}
#else
static inline struct rte_ether_addr* mt_eth_s_addr(struct rte_ether_hdr* eth) {
  return &eth->s_addr;
}

static inline struct rte_ether_addr* mt_eth_d_addr(struct rte_ether_hdr* eth) {
  return &eth->d_addr;
}
#endif

#if (JSON_C_VERSION_NUM >= ((0 << 16) | (13 << 8) | 0)) || \
    (JSON_C_VERSION_NUM < ((0 << 16) | (10 << 8) | 0))
static inline json_object* mt_json_object_get(json_object* obj, const char* key) {
  return json_object_object_get(obj, key);
}
#else
static inline json_object* mt_json_object_get(json_object* obj, const char* key) {
  json_object* value;
  int ret = json_object_object_get_ex(obj, key, &value);
  if (ret) return value;
  return NULL;
}
#endif

#endif
