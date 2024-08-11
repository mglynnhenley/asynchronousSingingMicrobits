#include "MicroBit.h"


#include "Synchronization.h"

#define MIN_TRIGGER_DELAY_TIME 10

XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

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

    ClockSync::Sync();
    while(true) {
        uBit->display.scroll("Hello World");

    }

    return 0;
}
