#include "MicroBit.h"


#include "Synchronization.h"

#define MIN_TRIGGER_DELAY_TIME 10

//
//void schedule_trigger() {
//  auto data_string = ubit.serial.readUntil("\r\n", MicroBitSerialMode::ASYNC);
//  if (data_string.length() != 6) {
//    return;
//  }
//  const uint8_t *data = (const uint8_t *)data_string.toCharArray();
//  size_t i = 0;
//
//  tlt::trigger_id_t trigger_id;
//  if (!tlt::parseHex<tlt::trigger_id_t>(data, &i, &trigger_id)) {
//    return;
//  }
//  tlt::trigger_delta_t trigger_delta;
//  if (!tlt::parseHex<tlt::trigger_delta_t>(data, &i, &trigger_delta)) {
//    return;
//  }
//
//  ubit.serial.send("[main] sending trigger\n");
//  tlt::node_trigger_event(trigger_id, trigger_delta);
//}
//
//void on_trigger(MicroBitEvent evt __attribute__((unused))) {
//  tlt::timestamp_t trigger_timestamp;
//  tlt::trigger_id_t trigger_id;
//  tlt::node_get_last_trigger(&trigger_id, &trigger_timestamp);
//
//  music_stop();
//
//  tlt::timestamp_t now = tlt::node_now();
//  tlt::time_delta_t delay = trigger_timestamp - now;
//  if (delay < MIN_TRIGGER_DELAY_TIME) {
//    ubit.serial.printf("[main] delay too small %d\n", delay);
//    return;
//  }
//  ubit.wait(delay);
//
//  music_play(&song_data, trigger_id);
//}
//
//void display_binary(uint8_t n, int row) {
//  for (int i = 0; i < 5; i++) {
//    if (n & (1 << i)) {
//      ubit.display.image.setPixelValue(i, row, 255);
//    }
//  }
//}
//
//int main() {
//  // watchdog_set_timeout(2000);
//  // watchdog_enable_checker(0);
//  // watchdog_enable_checker(1);
//  // watchdog_start();
//
//  ubit.seedRandom();
//  scheduler_init(ubit.messageBus);
//
//  music_init();
//  tlt::node_init();
//
//  ubit.messageBus.listen(MICROBIT_ID_TLT_NODE, TLT_NODE_EVT_TRIGGERS_AVAILABLE,
//                         on_trigger);
//
//  while (true) {
//    // watchdog_reset_checker(0);
//    fiber_sleep(100);
//
//    int status = tlt::node_status();
//    tlt::level_t level = tlt::node_level();
//    uint32_t last_adjust_abs = tlt::node_last_adjust_abs();
//
//    ubit.display.clear();
//
//    // show the status on the top row
//    display_binary(status, 0);
//
//    // show the abs adjust on the middle row
//    for (size_t i = 0; i < 5; i++) {
//      size_t cutoff = 1 << (i * 2);
//      if (last_adjust_abs > cutoff) {
//        ubit.display.image.setPixelValue(i, 2, 255);
//      }
//    }
//
//    // show the level on the bottom row
//    if (level > 0b11111) {
//      level = 0b11111;
//    }
//    display_binary(level, 4);
//
//    schedule_trigger();
//  }
//}

#include "note1.cpp"

#define MIN_TRIGGER_DELAY_TIME 10
#define ARTICULATION_MS 10
#define NUMBER_MICROBITS 3

int main() {
//    auto uBit = std::make_shared<MicroBit>();
////    scheduler_init(uBit->messageBus);
//
//    uBit->init();
//    uBit->display.clear();
//    uBit->serial.printf("hello world\r\n");
//    ClockSync::Init(uBit, 3);
//    uBit->serial.printf("found master\r\n");
//    ClockSync::Sync();
//    uBit->serial.printf("synced\r\n");
//    while(true) {
//        uBit->display.scroll("Hello World");
//
//    }


    auto uBit = std::make_shared<MicroBit>();
    Pin* pin_ = &uBit->audio.virtualOutputPin;
    uBit->init();
    ClockSync::Init(uBit, NUMBER_MICROBITS);

    bool isArticulated = false;
    int cur_note = 0;
    int fin_note = sizeof(_song_events)/sizeof(_song_events[0]);
    ClockSync::Sync();
    //int time = uBit->systemTime();
    int time = ClockSync::SystemTime();
    int next_change = time + (_song_events[0]).duration_ms;
    pin_ -> setAnalogValue((_song_events[0]).velocity);
    pin_ -> setAnalogPeriodUs((_song_events[0]).period_us);
    while (cur_note < fin_note) {
        //time = uBit->systemTime();
        time = ClockSync::SystemTime();
        if (next_change < time) {
            if (! isArticulated) {
                pin_ -> setAnalogValue(0);
                pin_ -> setAnalogPeriodUs(0);
                next_change = next_change + ARTICULATION_MS;
                isArticulated = true;
            }
            else {
                isArticulated = false;
                cur_note = cur_note + 1;
                if (cur_note >= fin_note) {
                    pin_ -> setAnalogValue(0);
                    pin_ -> setAnalogPeriodUs(0);
                }
                else {
                    pin_ -> setAnalogValue((_song_events[cur_note]).velocity);
                    pin_ -> setAnalogPeriodUs((_song_events[cur_note]).period_us);
                    next_change = next_change + (_song_events[cur_note]).duration_ms - ARTICULATION_MS;
                }
            }
        }
        fiber_sleep(1);
    }
    while(true) {
        uBit->display.scroll("Hello World");

    }

    return 0;
}
