/**
 * Depending on the type of package, there are different
 * compilation rules for this directory.  This comment applies
 * to packages of type "pkg."  For other types of packages,
 * please view the documentation at http://mynewt.apache.org/.
 *
 * Put source files in this directory.  All files that have a *.c
 * ending are recursively compiled in the src/ directory and its
 * descendants.  The exception here is the arch/ directory, which
 * is ignored in the default compilation.
 *
 * The arch/<your-arch>/ directories are manually added and
 * recursively compiled for all files that end with either *.c
 * or *.a.  Any directories in arch/ that don't match the
 * architecture being compiled are not compiled.
 *
 * Architecture is set by the BSP/MCU combination.
 */

#include "os/os_task.h"
#include "console/console.h"
#include "extadv_test/advertise_svc_priv.h"
#include "extadv_test/extadv_test.h"



#define ADVEXT_TESTER_STACK_SIZE (300)
#define ADVEXT_TESTER_TASK_PRIO         (129)
#define ADVEXT_TESTER_PAYLOAD_SIZE OS_ALIGN(MYNEWT_VAL(BLE_EXT_ADV_MAX_SIZE), 4)

#define ADVEXT_TESTER_NUM_BUFFS  (2)
#define ADVEXT_TESTER_POOL_SIZE (ADVEXT_TESTER_PAYLOAD_SIZE + \
                            sizeof(struct os_mbuf) + sizeof(struct os_mbuf_pkthdr))

static struct os_mempool advext_tester_adv_pool;
static struct os_mbuf_pool advext_tester_mbuf_pool;
static os_membuf_t advext_tester_mem[OS_MEMPOOL_SIZE(ADVEXT_TESTER_NUM_BUFFS, ADVEXT_TESTER_POOL_SIZE)];

struct os_task advext_task;
os_stack_t advext_tester_task_stack[ADVEXT_TESTER_STACK_SIZE];
struct os_eventq advext_evtq;
struct os_event advext_event;

uint8_t * test_pattern;

void advext_tester_cb(void* arg) {
    console_printf("adv done, calling it again\n");
    os_time_delay(OS_TICKS_PER_SEC/3);
    advext_tester_send();
}

void advext_tester_go(void* buf, uint16_t len, uint16_t duration, int max_events, uint16_t interval) {

    int ret;

    console_printf("broadcasting: length=%d\n", len);

    struct os_mbuf* mbuf = os_mbuf_get_pkthdr(&advext_tester_mbuf_pool, 0);
    if (mbuf == NULL) {
        console_printf("retry later, buffer in use\n");
        return;
    }
    ret = os_mbuf_append(mbuf, buf, len);

    assert(ret == 0);

    struct ble_gap_ext_adv_params adv_params;
    adv_params = (struct ble_gap_ext_adv_params){ 0 };
    adv_params.itvl_min = ADV_INT_EXT(interval);
    adv_params.itvl_max = ADV_INT_EXT(interval+10); // auto spread to 10ms to randomize
    adv_params.connectable = false;
    adv_params.legacy_pdu = false;
    adv_params.scannable = false;
    adv_params.primary_phy = 0x01; // 1M = 0x01, 2M = 0x02
    adv_params.secondary_phy = 0x01; // 1M for now, will test 2M later
    adv_params.tx_power = 127;

    advertise_svc_send(&adv_params, mbuf, duration, max_events, advext_tester_cb, NULL);
}

void advext_tester_handler(struct os_event* evt) {
    uint32_t os_tick;
    os_time_ms_to_ticks(100, &os_tick);
    os_time_delay(os_tick); // schedule some pause
    advext_tester_go(test_pattern, EXTADV_TEST_PATTERN_LEN, EXTADV_DURATION, EXTADV_MAX_EVENTS, EXTADV_INTERVAL);
}

void advext_tester_send() {
    bzero(&advext_event, sizeof(struct os_event));
    advext_event.ev_cb =  advext_tester_handler;
    advext_event.ev_arg = NULL;
    os_eventq_put(&advext_evtq, &advext_event); // queue it
}

void advext_tester_task_func(void *arg) {

    /* The task is a forever loop that does not return */
    while (1) {
        os_eventq_run(&advext_evtq);
    }
}

void advext_tester_init(void) {
    /* Initialize the task */

    int rc;


    // create test pattern

    int len = EXTADV_TEST_PATTERN_LEN;

    test_pattern = os_malloc(len);

    for(uint16_t i = 0; i < len; i++) {
        test_pattern[i] = (uint8_t )i; // fake data
    }

    bzero(&advext_event, sizeof(struct os_event));

    os_eventq_init(&advext_evtq);


    rc = os_mempool_init(&advext_tester_adv_pool, ADVEXT_TESTER_NUM_BUFFS, ADVEXT_TESTER_POOL_SIZE,
                         advext_tester_mem, "adv_client_ext_adv_mem");
    assert(rc == 0);
    rc = os_mbuf_pool_init(&advext_tester_mbuf_pool, &advext_tester_adv_pool,
                           ADVEXT_TESTER_POOL_SIZE, ADVEXT_TESTER_NUM_BUFFS);
    assert(rc == 0);

    os_task_init(&advext_task, "advext_tester", advext_tester_task_func, NULL, ADVEXT_TESTER_TASK_PRIO,
                 OS_WAIT_FOREVER, advext_tester_task_stack, ADVEXT_TESTER_STACK_SIZE);

}