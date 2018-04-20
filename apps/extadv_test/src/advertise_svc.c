//
// Created by Marcus Williford on 4/7/18.
//

#include <stdio.h>
#include <nimble/ble.h>
#include "host/ble_hs.h"
#include "os/os.h"
#include "host/util/util.h"
#include "console/console.h"
#include "sysinit/sysinit.h"
#include "extadv_test/advertise_svc_priv.h"

/* Define task stack and task object */
#define ADVERTISE_SVC_TASK_PRIO         (200)
#define ADVERTISE_SVC_STACK_SIZE       (800) // TODO:  Tune size
struct os_task advertise_svc_task;
static os_stack_t advertise_svc_task_stack[ADVERTISE_SVC_STACK_SIZE];
struct os_eventq g_advertise_svc_evq;

static void * advertise_svc_memory_buffer;
static struct os_mempool advertise_svc_mempool;

struct log advertise_svc_log;

static void ble_app_advertise(struct ble_gap_ext_adv_params *adv_params, struct os_mbuf* adv_data, int duration, int max_events);


static uint8_t own_addr_type;
struct advertise_svc_state {
    volatile bool running;
    struct os_event * curr_os_event; // store becuase not all paths have valid arg on callbacks
    struct os_mbuf* buff;
    struct os_mutex adv_lock;
};

static struct advertise_svc_state m_state = {
        .running = false,
        .curr_os_event = NULL,
};

/**
 * Lock access to access to m_state and curr_os_event
 */
int
advertise_svc_lock(void)
{
    int rc;

    rc = os_mutex_pend(&m_state.adv_lock, OS_TIMEOUT_NEVER);
    if (rc == 0 || rc == OS_NOT_STARTED) {
        return (0);
    }
    return (rc);
}

/**
 * Unlock sensor manager once the list of sensors has been accessed
 */
void
advertise_svc_unlock(void)
{
    (void) os_mutex_release(&m_state.adv_lock);
}

/**
 *
 * context: Nimble-land
 *
 * @param evt
 */
static void unwrap_evt_cb_free(struct os_event* evt) {
    advertise_svc_evt_t *aevt = evt->ev_arg;
    aevt->done(aevt->done_arg); // general notification callback
    os_memblock_put(&advertise_svc_mempool, aevt); // release advertise_svc_evt_t, which includes space for os_event
}
/**
 *
 * Start and/or replace currently running advertisement on instance 0
 * TODO: manage multiple instances in parallel
 *
 * @context advertise_svc task

 * @param evt
 */
inline static void do_advertise_handler(struct os_event * evt) {

    // stop any advertising that is ongoing
    if (ble_gap_adv_active()) {
        console_printf("stopped adv\n");
        ble_gap_ext_adv_stop(0);
    }
    ble_gap_ext_adv_remove(0); // unconfigure is needed, so we don't get assert on next configure, ok, to call if not conf

    // unwrap event
    advertise_svc_evt_t *aevt = evt->ev_arg;

    assert(advertise_svc_lock() == 0); // loc
//---------- lock
        if (m_state.running) { // store pointers to clean up, so nimble context does not remove as we adjust this state
           // call clean up
            assert(m_state.curr_os_event);
            unwrap_evt_cb_free(m_state.curr_os_event);
        } else {
            m_state.running = true;
        }
        m_state.curr_os_event = evt; // store to clean up when this evt is fully done or canceled

// ---------- unlock
    advertise_svc_unlock();

    ble_app_advertise(&aevt->adv_params, aevt->adv_data, aevt->duration, aevt->max_events);
}


/**
 *   Task --------
 *   This is the actual task which runs all advertising output.  The task will eventually help prioritize and handle requirements
 *   when multiple outbound advertisements need to take place.  Because extended advertising can handle multiplle 'instances'
 *   it is also possible that it would manage concurrent requests into the stack, but for now one at a time.
 *
 *
 *   @context advertise_svc task
 *
 */
void advertise_svc_task_func() {
    //hal_gpio_init_out(LED_2, 1);
    uint32_t os_ticks;


    while(1) { // rate limit a little, study this later.
        os_time_ms_to_ticks(50, &os_ticks);
        os_time_delay(os_ticks);

        struct os_event *ev;

        ev = os_eventq_get(&g_advertise_svc_evq);
        LOG_DEBUG(&advertise_svc_log, LOG_MODULE_DEFAULT,"advertise_svc get");

        // we are going to change state and start a new adv message
        do_advertise_handler(ev);


    }
}


static int app_gap_event_handler(struct ble_gap_event *event, void *arg);

/**
 * ble_app_advertise
 *
 * Cause an advertisement to happen at the ble layer
 *
 *  @param adv_params          params for adv
 *  @param adv_data          os_mbuf data which will be free'ed when this adv is stopped
 *  @param duration     duration of this advertisement
 *  @param max_events    not sure what this does, passing it downstream, TODO: Research max_events
 */
static void
ble_app_advertise(struct ble_gap_ext_adv_params *adv_params, struct os_mbuf* adv_data, int duration, int max_events)
{
    int rc;

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        console_printf("error determining address type; rc=%d\n", rc);
        return;
    }
    adv_params->own_addr_type = own_addr_type;

    rc = ble_gap_ext_adv_configure(0, adv_params, NULL, app_gap_event_handler, NULL );
    assert(rc == 0);
    rc = ble_gap_ext_adv_set_data(0, adv_data);
    assert(rc == 0);
    rc = ble_gap_ext_adv_start(0, duration/10, max_events);
    assert(rc == 0);
}




/**
 *
 * Send an advertisement data mbuf
 *
 * This will add the advertising request to the queue for sched
 * When advertisement is done or canceled, cb is called.
 *
 *
 * @param adv_params    extended adv params - we copy off, we don't hold a reference to this
 * @param adv_data      data for advertising - we hold a reference, clean up when we call cb
 * @param duration      Duration of adv event
 * @param max_events    not sure what this does, passed to hci - reserach it.
 * @param cb            void(*cb)(void* arg)
 * @param cbarg         Provided to callback so you can dispose of some additional datastructures if needed, put anything in here
 *
 *
 */
void advertise_svc_send(const struct ble_gap_ext_adv_params *adv_params, struct os_mbuf* adv_data, int duration, int max_events, void(*cb)(void* arg), void* arg) {
    static advertise_svc_evt_t *evt;
    evt = os_memblock_get(&advertise_svc_mempool);

    // too many stacked up, need to remove some or skip this one
    if (evt == NULL) {
        console_printf("error: too many advertise_svc requests\n");
        return;
    }
    bzero(evt, sizeof(advertise_svc_evt_t));
    evt->os_evt.ev_arg = evt; // pointer to top of struct
    evt->duration = duration;
    evt->done = cb;
    evt->done_arg = arg; // store an arg for callback
    evt->max_events = max_events;
    memcpy(&evt->adv_params, adv_params, sizeof(struct ble_gap_ext_adv_params));
    evt->adv_data = adv_data; // os_mbuf
    LOG_DEBUG(&advertise_svc_log, LOG_MODULE_DEFAULT," Adv event put`x`\n");

    os_eventq_put(&g_advertise_svc_evq, &evt->os_evt);
}

/**
 *
 *
 * @context nimble-land
 *
 * @param event
 * @param arg
 * @return
 */
static int
app_gap_event_handler(struct ble_gap_event *event, void *arg) {
    console_printf("evt %x\n", event->type);
    switch (event->type) {
        case BLE_GAP_EVENT_ADV_COMPLETE:
            LOG_DEBUG(&advertise_svc_log, LOG_MODULE_DEFAULT," Adv complete\n");
            assert(advertise_svc_lock() == 0); // lock
            if (m_state.running) {
                assert(m_state.curr_os_event != NULL);
                unwrap_evt_cb_free(m_state.curr_os_event);
                m_state.running = false;
                m_state.curr_os_event = NULL;
            }
            advertise_svc_unlock();
            return 0;
    }
    return 0;
}




void advertise_svc_init() {

    log_register("advertise_svc", &advertise_svc_log, &log_console_handler, NULL, LOG_SYSLEVEL);

    LOG_INFO(&advertise_svc_log, LOG_MODULE_DEFAULT,"advertise_svc Init\n");

    // used by create event for buffer
    advertise_svc_memory_buffer = os_malloc(
            OS_MEMPOOL_BYTES(MYNEWT_VAL(POOL_SIZE), sizeof(advertise_svc_evt_t)));

    os_mempool_init(&advertise_svc_mempool, MYNEWT_VAL(POOL_SIZE), sizeof(advertise_svc_evt_t), advertise_svc_memory_buffer,
                    "advertise_svc_pool");

    os_mutex_init(&m_state.adv_lock);


    os_eventq_init(&g_advertise_svc_evq);

    /* Initialize the task */
    os_task_init(&advertise_svc_task, "adv_svc", advertise_svc_task_func, NULL, ADVERTISE_SVC_TASK_PRIO,
                 OS_WAIT_FOREVER, advertise_svc_task_stack, ADVERTISE_SVC_STACK_SIZE);


}