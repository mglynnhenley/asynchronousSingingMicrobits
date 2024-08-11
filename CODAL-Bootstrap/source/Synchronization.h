
#include "MicroBit.h"
#include <stdint.h>

#include <memory>
#include <set>
#include <functional>

/*
    Main idea:
        Simply perform a PTP, that is
            1) Choose a master in the call to the constructor
            2) Sync method will execute a PTP

        I was thinking of implementing it using MicroBits Radio interface:
            1) MicroBitRadioDatagram.h (Higher level)
                https://github.com/lancaster-university/codal-microbit-v2/blob/master/inc/MicroBitRadioDatagram.h
            2) MicroBitRadio.h
                https://github.com/lancaster-university/codal-microbit-v2/blob/master/inc/MicroBitRadio.h


*/



namespace ClockSync {
typedef uint32_t timestamp_t;
typedef uint32_t serial_t;

struct PTP_packet
{
    uint8_t flag;
    serial_t serial;
    timestamp_t timestamp;
};

/*
     Pre:
        At least two microbits are required
        All microbits either use method (1) or all use method (2)
        if all microbits use method (1), then exactly one is called with is_master = true
    Post:
        initialization of the network as well provide an agreement on the choice of the master
    Ideas for choosing the master:
        1)  Specify the master by simply initializing one of the microbits
            with is_master set to true
        2)  Choose the master by exchanging some information across the
            network and choose the one that satisfies some property (I was
            thinking of choosing the one with smallest serial number, as it
            is fairly easy to implement and serial number provides uniqueness)
*/
//    ClockSync(MicroBit &uBit, int num_of_microbits, bool is_master); // (1)

void Init(std::shared_ptr<MicroBit> uBit, int num_of_microbits);                 // (2)

/*
    Pre:
        Master has been already chosen
    Post:
        Microbits clocks are synchronized with the master's clock
        Any microbit is allowed to leave the routine iff all microbits have already entered the
   synchronization Overview: 1) Master broadcasts its current time across the network 2) Each
   microbit saves the time it received the timestamp from the master (Sync) 3) Each microbit
   pings the master and receives the time the master received the ping (Delay_Req, Delay_Resp)
        4) Having the timestamps, calculate the offset and adjust the clock
    Details:
        https://en.wikipedia.org/wiki/Precision_Time_Protocol
    Coments:
        1) should be performed iff all microbits entered synchronization (This property is
   crucial for providing a barrier sync) The times of microbits leaving the sync method might
   hugely vary
*/
void Sync();

/*
    Post:
        returns adjusted system time of a microbit
    Note:
        function returns uBit.systemTime() + offset, where offset is calculated
        using the synchronization protocol
*/
timestamp_t SystemTime();

// -------------------------------------------------------------------


/*
    Sends a PTP_packet with given params
*/
void send(uint8_t flag, uint32_t serial, timestamp_t timestamp);

/*
    Sync subroutines designed for master and followers respectively
*/
void SyncAsMaster();

void SyncAsFollower();

/*
    Event used to handle initial exchange of serial numbers, that is
    receive num_of_microbits - 1 packets and comapare incoming serial
    numbers
*/
void master_selection(MicroBitEvent e);

/*
    Used to store initial timestamp broadcasted by the master
*/
void on_sync(MicroBitEvent e);

/*
    Master's event handling the delay_req ping sent by a follower
    Upon receiving a ping, it should respond with the time it received
    the packet
*/
void on_delay_req(MicroBitEvent e);

/*
    Follower should use this event, after pinging the master, in
    order to receive the delay_resp ping
*/
void on_delay_resp(MicroBitEvent e);
};
