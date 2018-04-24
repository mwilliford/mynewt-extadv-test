
//#define NRF52840_XXAA (1)
#include <nimble/ble.h>
#include <stdio.h>
#include "sysinit/sysinit.h"
#include "os/os.h"
#include "os/os_task.h"
#include "console/console.h"
#include "host/ble_hs.h"
#include "hal/hal_gpio.h"
#include "bsp/bsp.h"
#include "hal/hal_timer.h"
#include "os/os_time.h"
#include "datetime/datetime.h"
#include "host/util/util.h"
#include "cbmem/cbmem.h"
#include <bsp/bsp.h>
#include "extadv_test/extadv_test.h"

/* BLE */
#include "nimble/ble.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "extadv_test/extadv_test.h"

struct ble_gap_ext_disc_params disc_params;



#define MY_TASK_PRIO         (128)
#define MY_STACK_SIZE       (64)
#define MAX_CBMEM_BUF 2048

struct os_task my_task;


void
print_addr(const void *addr)
{
    const uint8_t *u8p;

    u8p = addr;
    console_printf("%02x:%02x:%02x:%02x:%02x:%02x",
                   u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);
}

static void
ble_app_set_addr(void)
{
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

}

static void
ble_observer_decode_adv_data(uint8_t *adv_data, uint8_t adv_data_len, int status)
{
    //struct ble_hs_adv_fields fields;

    console_printf(" length_data=%d data=", adv_data_len);
    //print_bytes(adv_data, adv_data_len);
    for (int i = 0; i < adv_data_len; i++) {
        console_printf("%02x ", adv_data[i]);
    }
    console_printf("\n");

    //int expected = EXTADV_TEST_PATTERN_LEN;
    if (status == 0 ) { // completed
        console_printf("*************************************** completed ******************************");
    }
//    if (EXTADV_TEST_PATTERN_LEN != adv_data_len) {
//        console_printf("********* NO SIZE MATCH - BAD!  expected = %d : was = %d\n", expected, adv_data_len);
//        return;
//    }
//
//    if (strncmp((char*)test_pattern, (char*)adv_data, expected) != 0) {
//        console_printf("********* NO MATCH - BAD!\n");
//    } else {
//        console_printf("good match\n");
//    }
}


static void
ble_observer_decode_event_type(struct ble_gap_ext_disc_desc *desc)
{
    uint8_t directed = 0;

    if (desc->props & BLE_HCI_ADV_LEGACY_MASK) {
        return;
    }

    console_printf("Extended adv: ");
    if (desc->props & BLE_HCI_ADV_CONN_MASK) {
        console_printf("'conn' ");
    }
    if (desc->props & BLE_HCI_ADV_SCAN_MASK) {
        console_printf("'scan' ");
    }
    if (desc->props & BLE_HCI_ADV_DIRECT_MASK) {
        console_printf("'dir' ");
        directed = 1;
    }
    if (desc->props & BLE_HCI_ADV_SCAN_RSP_MASK) {
        console_printf("'scan rsp' ");
    }

    switch(desc->data_status) {
        case 0x00:
            console_printf("completed");
            break;
        case 0x01:
            console_printf("incompleted");
            break;
        case 0x02:
            console_printf("truncated");
            break;
        case BLE_HCI_ADV_DATA_STATUS_MASK:
            console_printf("mask");
            break;
        default:
            console_printf("reserved %d", desc->data_status);
            break;
    }

    console_printf(" rssi=%d txpower=%d, pphy=%d, sphy=%d, sid=%d,"
                           " addr_type=%d addr=",
                   desc->rssi, desc->tx_power, desc->prim_phy, desc->sec_phy,
                   desc->sid, desc->addr.type);
    print_addr(desc->addr.val);
    if (directed) {
        console_printf(" init_addr_type=%d inita=", desc->direct_addr.type);
        print_addr(desc->direct_addr.val);
    }

    console_printf("\n");

    if(!desc->length_data) {
        return;
    }

    ble_observer_decode_adv_data(desc->data, desc->length_data, desc->data_status);
}


static int
ble_observer_gap_event(struct ble_gap_event *event, void *arg) {

    switch (event->type) {
        case BLE_GAP_EVENT_EXT_DISC: {

            ble_observer_decode_event_type(&event->ext_disc);

            return 0;
        }
        case BLE_GAP_EVENT_DISC_COMPLETE:
            console_printf("disc complete\n");
            return 0;
        default:
            console_printf("other event %d", event->type);
            assert(false);
    }
    return 0;

}

static void
start_observing() {
    int rc;
    struct ble_gap_ext_disc_params uncoded = {0};

    uncoded.window = disc_params.window = 30;
    uncoded.itvl = disc_params.itvl = 50;
    uncoded.passive = true;

    uint8_t own_addr_type;

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        console_printf("error determining address type; rc=%d\n", rc);
        return;
    }

    rc = ble_gap_ext_disc(own_addr_type, 0, 0, 0, BLE_HCI_SCAN_FILT_NO_WL, 0,
                          &uncoded, NULL, ble_observer_gap_event, NULL);
    assert(rc == 0);
}

static void
ble_on_sync(void)
{
    /* Generate a non-resolvable private address. */
    console_printf("ble_app_on_sync\n");
    ble_app_set_addr();
    if (true) {
        start_observing();
    }
    if (false) {
        advext_tester_send();
    }
}

int
main(int argc, char **argv)
{


    ble_hs_cfg.sync_cb = ble_on_sync;


    sysinit();


    /* Initialize the NimBLE host configuration. */
    log_register("ble_hs", &ble_hs_log, &log_console_handler, NULL,
                 LOG_LEVEL_DEBUG);


    /* As the last thing, process events from default event queue. */
    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
}