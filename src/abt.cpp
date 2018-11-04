#include "../include/simulator.h"
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <queue>

using namespace std;
/* ******************************************************************
 ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.1  J.F.Kurose

   This code should be used for PA2, unidirectional data transfer 
   protocols (from A to B). Network properties:
   - one way network delay averages five time units (longer if there
     are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
     or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
     (although some can be lost).
**********************************************************************/

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/
#define TIME_OUT 20.0f

queue<string> waiting_queue;

int get_checksum(struct pkt *packet) {
    int cs = 0;
    cs += packet->seqnum;
    cs += packet->acknum;
    for (int i = 0; i < 20; i++) {
        cs += packet->payload[i];
    }
    return ~cs;
}

int reverse(int num) {
    return 1 - num;
}

enum State {
    WAITING_ACK,
    WAITING_MSG,
    SENDING_WAITING_QUEUE
};

struct SenderInfo {
    int next_seq;
    int next_ack;
    enum State sender_state;
    struct pkt next_packet;

} A;

struct ReceiverInfo {
    int next_seq;
    struct pkt ack_pkt;
} B;

/* called from layer 5, passed the data to be sent to other side */
void A_output(struct msg message) {
    if (A.sender_state == WAITING_ACK) {
        waiting_queue.push(string(message.data));
        return;
    }
    //add pkt info: get seqnum, acknum, compute checksum, copy msg to payload
    A.next_ack = A.next_seq;

    A.next_packet.seqnum = A.next_seq;
    A.next_packet.acknum = A.next_ack;
    memcpy(A.next_packet.payload, message.data, sizeof(message.data));
    int ckecksum = get_checksum(&A.next_packet);
    A.next_packet.checksum = ckecksum;
    //then send pkt to layer 3 using tolayer3()
    cout << "Send: " << message.data << ", seq=" << A.next_seq << " ack=" << A.next_ack << endl;
    tolayer3(0, A.next_packet);
    //notice that timer need to be operate
    starttimer(0, TIME_OUT);
    A.sender_state = WAITING_ACK;
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet) {
    if (A.sender_state != WAITING_ACK) {
        return;
    }
    //receive ack from receiver, whether retransmit or send next pkt
    //if received 2 same ack, do nothing for the second one
    if (packet.checksum != get_checksum(&packet) || packet.acknum != A.next_ack) {
        return;
    }

    if (packet.acknum == A.next_ack) {
        stoptimer(0);
        A.next_seq = reverse(A.next_seq);
        if (!waiting_queue.empty()) {
            string next_msg = waiting_queue.front();
            waiting_queue.pop();
            struct msg next_message;
            memcpy(next_message.data, (char *) next_msg.data(), 20);
            A.sender_state = SENDING_WAITING_QUEUE;
            A_output(next_message);
            return;
        }
        A.sender_state = WAITING_MSG;
    }

}

/* called when A's timer goes off */
void A_timerinterrupt() {
    if (A.sender_state != WAITING_ACK) {
        return;
    }
    //retransmit pkt
    cout << "Resend: " << A.next_packet.payload << ", seq=" << A.next_seq << " ack=" << A.next_ack << endl;
    tolayer3(0, A.next_packet);
    starttimer(0, TIME_OUT);
    A.sender_state = WAITING_ACK;
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init() {
    //some initialization of global parameters, like seqnum, acknum
    A.sender_state = WAITING_MSG;
    A.next_ack = 0;
    A.next_seq = 0;
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet) {
    //3 cases:
    //case 1: perfect pkt, not received before, transmit ack and call tolayer5()
    //case 2: perfect pkt, but received before, transmit ack but don't deliver to upper layer
    int checksum = get_checksum(&packet);
    if (checksum == packet.checksum) {
        cout << "Receive: " << packet.payload << ", seq=" << packet.seqnum << " ack=" << packet.acknum << endl;
        if (packet.seqnum == B.next_seq) {
            tolayer5(1, packet.payload);
            B.ack_pkt.acknum = packet.seqnum;
            B.ack_pkt.checksum = get_checksum(&B.ack_pkt);
            tolayer3(1, B.ack_pkt);
            B.next_seq = reverse(B.next_seq);
            cout << "Response to sender: " << "ack=" << B.ack_pkt.acknum << " next expected seq=" << B.next_seq << endl;
        } else {
            B.ack_pkt.acknum = packet.seqnum;
            B.ack_pkt.checksum = get_checksum(&B.ack_pkt);
            cout << "Response to sender: " << "ack=" << B.ack_pkt.acknum << " next expected seq=" << B.next_seq << endl;
            tolayer3(1, B.ack_pkt);
        }
    } else {
        cout << "Broken packet" << endl;
    }

//    else {
//        //case 3: bad pkt, transmit ack with error info
//        B.ack_pkt.acknum = reverse(B.next_seq);
//        tolayer3(1, B.ack_pkt);
//    }

}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init() {
    //some initialization of global parameters
    B.next_seq = 0;
    B.ack_pkt.seqnum = 0;
    memset(B.ack_pkt.payload, 0, 20);
}
