
#ifndef __ADVERTISE_CLIENT_PRIV_H__
#define __ADVERTISE_CLIENT_PRIV_H__

#include "os/os.h"
#include "os/os_mempool.h"
#include "sysinit/sysinit.h"
#include "host/ble_hs.h"



#define ADV_INT(_ms) ((_ms) * 8 / 5)
#define ADV_INT_EXT(_ms) ((_ms) * 1000 / BLE_HCI_ADV_ITVL)

void adv_lightevent_listener_init();
void adv_ext_event_listener_init();
void advertise_svc_send(const struct ble_gap_ext_adv_params *adv_params, struct os_mbuf* adv_data, int duration, int max_events, void(*cb)(void* arg), void* arg);
typedef struct advertise_svc_evt {
    struct os_event os_evt;
    struct ble_gap_ext_adv_params adv_params;
    struct os_mbuf* adv_data;
    void(*done)(void* arg);
    void* done_arg;
    int duration;
    int max_events;
} advertise_svc_evt_t;



#endif