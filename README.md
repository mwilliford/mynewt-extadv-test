#Extended Advertisement Test

##Instructions

- build as usual
- I tested on nrf52832 (i don't have too many of the 840's yet)
- Deploy on 2 devices, so they talk back and forth.  They both trasnmit, and both receive extended adv packets.
- The test software will print the results of observed packets, and note if they match what was sent.

## Summary of Issues
- Initially, lots of asserts.
- Apply diff to nimble, maybe a little better, heavy corruption though.
- Issue with scanning stopping after 5-10min.  I see no BLE_GAP_EVENT_DISC_COMPLETE when it stops.  I know it stops because r
  resetting the 'other' board does not fix it.  Only resetting the board which stops decoding messages fixes it.


## Details

Initially I have the following stack trace and asserts (master branch 42826d51309db2aaa1fe6f50000ca19d6ab9cd12) of nimble:

(gdb) bt
#0  __assert_func (file=0x0, line=0, func=<optimized out>, e=<optimized out>) at repos/apache-mynewt-core/kernel/os/src/arch/cortex_m4/os_fault.c:137
#1  0x0001c356 in ble_ll_adv_aux_schedule_next (advsm=0x0, advsm@entry=0x20007444 <g_ble_ll_adv_sm>) at repos/apache-mynewt-nimble/nimble/controller/src/ble_ll_adv.c:1150
#2  0x0001cb90 in ble_ll_adv_aux_schedule (advsm=advsm@entry=0x20007444 <g_ble_ll_adv_sm>) at repos/apache-mynewt-nimble/nimble/controller/src/ble_ll_adv.c:1304
#3  0x0001ce9a in ble_ll_adv_reschedule_event (advsm=advsm@entry=0x20007444 <g_ble_ll_adv_sm>) at repos/apache-mynewt-nimble/nimble/controller/src/ble_ll_adv.c:2894
#4  0x0001cf4c in ble_ll_adv_sec_done (advsm=0x20007444 <g_ble_ll_adv_sm>) at repos/apache-mynewt-nimble/nimble/controller/src/ble_ll_adv.c:3191
#5  0x0001cfe4 in ble_ll_adv_sec_event_done (ev=<optimized out>) at repos/apache-mynewt-nimble/nimble/controller/src/ble_ll_adv.c:3197
#6  0x00009430 in os_eventq_run (evq=evq@entry=0x200073a8 <g_ble_ll_data+44>) at repos/apache-mynewt-core/kernel/os/src/os_eventq.c:150
#7  0x0001b66a in ble_ll_task (arg=<optimized out>) at repos/apache-mynewt-nimble/nimble/controller/src/ble_ll.c:1099
#8  0x00000000 in ?? ()

Which PR #43 might address:
https://github.com/apache/mynewt-nimble/pull/43/files?diff=unified

However, I have also found that additional assets occur on a regular basis, and the attached diff might also help.  However, It might help at the increase of errors, meaning there are a quite a few unmatching packets reported to the upperlayer, which don't match what was transmitted.  I don't have a good enough sniffer to verify if they are corrupted tx or rx, but most of these asserts deal with tx, so my guess is that we tx them wrong.

See nimble.diff for some of the attempts I have made to improve it (however still with lots of unmatching packets).

cd repos/apache-mynewt-nimble
git apply ../../nimble.diff

I'm on slack to discuss (mwilliford)

