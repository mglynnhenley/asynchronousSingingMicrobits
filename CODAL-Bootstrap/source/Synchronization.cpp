

#include "Synchronization.h"

// FLAGS
#define PTP_PACKET_SIZE 9
#define SYNC_PING 0
#define DELAY_REQ 1
#define DELAY_RESP 2
#define MASTER_SELECTION 3
#define EMPTY_FIELD 0
#define SET_UNBLOCK_TIME 4
/*
Each packet will be sent with the flag at the begining specyfing the purpose of the message
    SYNC_PING
        initial timestamp broadcasted by the master in PTP
    DELAY_REQ
        message from slave to master, part of PTP
    DELAY_RESP
        message from master to slave containing the time of arrival of DELAY_REQ
    MASTER_SELECTION
        used in intial phase to agree on the master
    READY_PING
        indicating readiness for synchronization

    Packet:
        FLAG | SERIAL_NUMBER | TIMESTAMP
        FLAG: u_int8_t
        PAYLOAD:
            serial number:  uint32_t (microbit_serial_number())
            timestamp: unsigned long (32 bits, 4 bytes)
*/
// TODO: clear the message queue before the timing sync stuff
// TODO: somehow make sure we know all followers received the `time_to_unlock`

namespace {

std::shared_ptr<MicroBit> uBit;
size_t num_of_microbits;
int offset;
uint32_t serial_number;

// used for master selection
bool is_master;
uint32_t lowest_serial;
volatile size_t num_of_serials_received;

// used for sync
volatile bool delay_resp_received, sync_received;
ClockSync::timestamp_t ping_departure, ping_delay;
ClockSync::timestamp_t sync_timestamp, sync_arrival;
int num_of_pings;

std::set<ClockSync::serial_t> discovered_serials;
volatile bool follower_has_synced;

volatile ClockSync::timestamp_t time_to_unblock;
volatile bool unblock_pkt_received;
}

namespace ClockSync {
const timestamp_t UNBLOCK_DELAY = 500;

timestamp_t SystemTime()
{
    return uBit->systemTime() + offset;
};

/*
    Post:
        Send a packet of form
            FLAG | SERIAL_NUMBER | TIMESTAMP
*/
void send(uint8_t flag, uint32_t serial, timestamp_t timestamp)
{
    uint8_t buf[PTP_PACKET_SIZE];
    buf[0] = flag;
    buf[1] = serial >> 24;
    buf[2] = serial >> 16;
    buf[3] = serial >> 8;
    buf[4] = serial;
    buf[5] = timestamp >> 24;
    buf[6] = timestamp >> 16;
    buf[7] = timestamp >> 8;
    buf[8] = timestamp;
    uBit->radio.datagram.send(buf, 9);
}

void Init(std::shared_ptr<MicroBit> u, int n)                 // (2)
{
    delay_resp_received = false;
    sync_received = false;
    num_of_serials_received = 0;
    offset = 0;
    num_of_pings = 0;

    uBit = std::move(u);
    num_of_microbits = n;

    // not sure how microbit_serial_number works but it in one of the samples
    lowest_serial = microbit_serial_number();
    serial_number = lowest_serial;
    uBit->messageBus.listen(MICROBIT_ID_RADIO, MICROBIT_RADIO_EVT_DATAGRAM, master_selection, MESSAGE_BUS_LISTENER_IMMEDIATE);
    uBit->radio.enable();
    uBit->radio.setGroup(3);

    // This protocol for choosing master is fallible
    // it might be the case that some microbits propagated their serial number before
    // some of them enabled their radio, for now I will just force the microbits to wait
    // some time before transmitting their serial. We have to come up with better solution
//    uBit->sleep(60000); // sleeping for a minute

    timestamp_t sync_end = SystemTime() + 1000;
    do {
        send(MASTER_SELECTION, serial_number, EMPTY_FIELD);
        uBit->sleep(100);
    } while (!((num_of_serials_received >= num_of_microbits - 1) && SystemTime() > sync_end));
    send(MASTER_SELECTION, serial_number, EMPTY_FIELD);
    uBit->messageBus.ignore(MICROBIT_ID_RADIO, MICROBIT_RADIO_EVT_DATAGRAM, master_selection);

    is_master = lowest_serial == serial_number;
    uBit->serial.printf(is_master ? "I'm master\r\n" : "I'm follower\r\n");
}



std::unique_ptr<PTP_packet> toPTP_packet(const uint8_t *buffer)
{
    auto packet = std::make_unique<PTP_packet>();

    packet->flag = buffer[0];
    packet->serial = (buffer[1] << 24) + (buffer[2] << 16) + (buffer[3] << 8) + buffer[4];
    packet->timestamp = (buffer[5] << 24) + (buffer[6] << 16) + (buffer[7] << 8) + buffer[8];
    return packet;
}

void master_selection(MicroBitEvent e)
{
    uint8_t buffer[9];
    uBit->radio.datagram.recv(buffer, PTP_PACKET_SIZE);
    auto p = toPTP_packet(buffer);


    if (p->serial == serial_number) {
        return;
    }

    if (p->flag == MASTER_SELECTION)
    {
        discovered_serials.insert(p->serial);
        if (p->serial < lowest_serial)
            lowest_serial = p->serial;
        num_of_serials_received = discovered_serials.size();
    }
}

//void on_sync(MicroBitEvent e)
//{
//    sync_arrival = uBit->systemTime();
//    uint8_t buffer[9];
//    uBit->radio.datagram.recv(buffer, PTP_PACKET_SIZE);
//    auto p = toPTP_packet(buffer);
//
//    if (p->flag == SYNC_PING && p->serial == serial_number) {
//        // if received SYNC_PING, simply save the time of arrival and break the loop in main thread
//        sync_timestamp = p->timestamp;
//        sync_received = true;
//    }
//}

void on_delay_req(MicroBitEvent e)
{
    ClockSync::timestamp_t t = uBit->systemTime();
    uint8_t buffer[9];
    uBit->radio.datagram.recv(buffer, PTP_PACKET_SIZE);
    std::unique_ptr<PTP_packet> p = toPTP_packet(buffer);
    if (p->flag == DELAY_REQ)
    {
        // if received DELAY_REQ ping respond with the time of packet's arrival
        send(DELAY_RESP, p->serial, t);
        follower_has_synced = true;

    }
}

//void on_delay_resp(MicroBitEvent e)
//{
//    uint8_t buffer[9];
//    uBit->radio.datagram.recv(buffer, PTP_PACKET_SIZE);
//    std::unique_ptr<PTP_packet> p = toPTP_packet(buffer);
//
//    // If received response and it is directed to me
//    if (p->flag == DELAY_RESP && p->serial == serial_number)
//    {
//        ping_delay = p->timestamp;
//        delay_resp_received = true; // used to break while
//    }
//}


void SyncAsMaster()
{
    std::set<serial_t> followers;


//    master --> follower
//    SYNC_PING -->
//    <-- DELAY_REQ
//    DELAY_RESP -->
    uBit->messageBus.listen(MICROBIT_ID_RADIO, MICROBIT_RADIO_EVT_DATAGRAM, on_delay_req, MESSAGE_BUS_LISTENER_IMMEDIATE);
    for (serial_t follower_serial : discovered_serials) {
        follower_has_synced = false;
        while (!follower_has_synced) {
            send(SYNC_PING, follower_serial, uBit->systemTime());
            uBit->sleep(500);
        }
    }
    uBit->messageBus.ignore(MICROBIT_ID_RADIO, MICROBIT_RADIO_EVT_DATAGRAM, on_delay_req);


    timestamp_t time_to_unblock = SystemTime() + UNBLOCK_DELAY;
//    uBit->sleep(300);
    send(SET_UNBLOCK_TIME, serial_number, time_to_unblock);
//    uBit->sleep(300);
//    send(SET_UNBLOCK_TIME, serial_number, time_to_unblock);
//    uBit->sleep(300);
//    send(SET_UNBLOCK_TIME, serial_number, time_to_unblock);

    uBit->sleep(time_to_unblock - SystemTime());
}


void follower_listener(MicroBitEvent e) {
    timestamp_t t = uBit->systemTime();
    uint8_t buffer[9];
    uBit->radio.datagram.recv(buffer, PTP_PACKET_SIZE);
    std::unique_ptr<PTP_packet> p = toPTP_packet(buffer);

//    uBit->serial.printf("got %d pkt\r\n", p->flag);


    if (p->flag == SYNC_PING && p->serial == serial_number) {
        sync_arrival = t;
        // if received SYNC_PING, simply save the time of arrival and break the loop in main thread
        sync_timestamp = p->timestamp;
        sync_received = true;
    } else if (p->flag == SET_UNBLOCK_TIME) {
        time_to_unblock = p->timestamp;
        unblock_pkt_received = true; // used to break while
    } else if (p->flag == DELAY_RESP && p->serial == serial_number){
        ping_delay = p->timestamp;
        delay_resp_received = true; // used to break while
    }
}
void SyncAsFollower()
{

    sync_received = false;
    delay_resp_received = false;
    unblock_pkt_received = false;

    uBit->messageBus.listen(MICROBIT_ID_RADIO, MICROBIT_RADIO_EVT_DATAGRAM, follower_listener, MESSAGE_BUS_LISTENER_IMMEDIATE);

    // Waiting for sync ping from master

    while (!sync_received)
        uBit->sleep(100);
//    uBit->serial.printf("got sync pkt\r\n");

    // Start listening for a response from master to req ping


    // send a DELAY_REQ ping and save the time of departure
    ping_departure = uBit->systemTime();
    send(DELAY_REQ, serial_number, EMPTY_FIELD);

    while (!delay_resp_received)
        uBit->sleep(100);
//    uBit->serial.printf("got delay resp pkt\r\n");
    /*
        OFFSET CALCULATIONS
            Using notation from:
                https://en.wikipedia.org/wiki/Precision_Time_Protocol#Synchronization
            T1     - sync_timestamp
            T1'    - sync_arrival
            T2     - ping_departure
            T2'    - ping_delay
            offset = 1/2(T1' - T1 - T2' + T2)
    */
    offset = -((int)(sync_arrival - sync_timestamp) + (int)(ping_departure - ping_delay)) / 2;
//    uBit->serial.printf("sync_arrival %d sync_timestamp %d (%d)\r\n", sync_arrival, sync_timestamp, sync_arrival-sync_timestamp);
//    uBit->serial.printf("ping_departure %d ping_delay %d (%d)\r\n", ping_departure, ping_delay, ping_departure-ping_delay);
    while (!unblock_pkt_received)
        uBit->sleep(100);
    uBit->serial.printf("got unblock time %d, offset %d, (%d), (%d)\r\n", time_to_unblock, offset, SystemTime(), time_to_unblock - SystemTime());

//    while (true) {
//        uint8_t buffer[9];
//        auto recv_data_len = uBit->radio.datagram.recv(buffer, PTP_PACKET_SIZE);
//        if (recv_data_len == DEVICE_INVALID_PARAMETER | recv_data_len != 9) {
//            uBit->sleep(5);
//            continue;
//        }
//        std::unique_ptr<PTP_packet> p = toPTP_packet(buffer);
//
//        // If received response and it is directed to me
//        if (p->flag == SET_UNBLOCK_TIME)
//        {
//            time_to_unblock = p->timestamp;
//            break;
//        }
//    }
    uBit->messageBus.ignore(MICROBIT_ID_RADIO, MICROBIT_RADIO_EVT_DATAGRAM, follower_listener);
    uBit->sleep(time_to_unblock - SystemTime());
}

void Sync()
{
    if (is_master) {
        SyncAsMaster();
    } else {
        SyncAsFollower();
    }
}
}

/*
Useful links:
https://heptapod.host/flowblok/thought-leadering-time
https://github.com/tcode2k16/group-design-practical
https://github.com/lancaster-university/codal-microbit-v2
https://github.com/lancaster-university/microbit-v2-samples
https://www.i-programmer.info/programming/hardware/14390-microbit-morse-transmitter.html?start=1
*/
