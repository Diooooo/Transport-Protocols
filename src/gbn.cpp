#include "../include/simulator.h"
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <queue>
#include <string>
#include <vector>

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

queue<struct msg> waiting_queue;
vector<struct msg> window_buffer;

int head; //seq of first pkt in window
int tail; //seq of last pkt in window

int get_checksum(struct pkt *packet) {
    int cs = 0;
    cs += packet->seqnum;
    cs += packet->acknum;
    for (int i = 0; i < 20; i++) {
        cs += packet->payload[i];
    }
    return ~cs;
}

int next_num(int num, int window_size) {
    // compute next seq or ack
    return (num + 1) % window_size;
}

enum State {
    WAITING_ACK, //window is full
    WAITING_MSG, //window is not full
    SENDING_BUFFER //sending pkt in window
};

struct SenderInfo {
    int next_seq;
    int next_ack;
    enum State sender_state;
    struct pkt next_packet;
    int WINDOW_SIZE;
} A;

struct ReceiverInfo {
    int next_seq;
    struct pkt ack_pkt;
} B;

/* called from layer 5, passed the data to be sent to other side */
void A_output(struct msg message) {
    if (A.sender_state == WAITING_ACK) {
        waiting_queue.push(message);
        return;
    }
    if (A.sender_state == WAITING_MSG) { //window is not full, message from upper layer can be directly sent
        if (window_buffer.empty()) {
            starttimer(0, TIME_OUT); //start timer for the first pkt in window
            tail = head;
        } else {
            if (window_buffer.size() == A.WINDOW_SIZE - 1) {
                A.sender_state = WAITING_ACK;
            } else {
                cout << "Window not full, send msg" << endl;
            }
            tail = next_num(tail, A.WINDOW_SIZE);
        }
        window_buffer.push_back(message);
    }
    //send pkt once a time, let A_input control the rest steps
    A.next_packet.seqnum = A.next_seq;
    A.next_packet.acknum = A.next_ack;
    memcpy(A.next_packet.payload, message.data, sizeof(message.data));
    int ckecksum = get_checksum(&A.next_packet);
    A.next_packet.checksum = ckecksum;
    //then send pkt to layer 3 using tolayer3()
    cout << "Send: " << message.data << ", seq=" << A.next_seq << " ack=" << A.next_ack << endl;
    tolayer3(0, A.next_packet);
    A.next_seq = next_num(A.next_seq, A.WINDOW_SIZE);
    A.next_ack = A.next_seq;
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet) {
    //do nothing for broken pkt
    if (packet.checksum != get_checksum(&packet)) {
        return;
    }
    //case 1: receive ack of first pkt in window--shuffle window, send next pkt in window, and restart timer
    //case 2: unexpected ack--ignore
    if (packet.acknum == head) {
        cout << "received expect pkt" << endl;
        stoptimer(0);
        //shuffle window
        head = next_num(head, A.WINDOW_SIZE);
        window_buffer.erase(window_buffer.begin());
        if (!waiting_queue.empty()) {
            cout << "waiting queue not empty, shuffle window" << endl;
            tail = next_num(tail, A.WINDOW_SIZE);
            struct msg next_msg = waiting_queue.front();
            window_buffer.push_back(next_msg);
            waiting_queue.pop();
//            struct msg next_pkt;
//            memcpy(next_pkt.data, (char *) next_msg.data(), 20);
            A.sender_state = SENDING_BUFFER;
            A_output(next_msg);
            A.sender_state = WAITING_ACK;
        } else {
            cout << "waiting queue is empty, means window buffer is not full" << endl;
            A.sender_state = WAITING_MSG;
            if (window_buffer.empty()) {
                return;
            }
        }
        starttimer(0, TIME_OUT);

    } else {
        cout << "ack=" << packet.acknum << " ,ignore, need to retransmit later" << endl;
    }
}

/* called when A's timer goes off */
void A_timerinterrupt() {
    //retransmit all N pkt(if queue.size<N, retransmit all pkt in queue)
    cout << "Resend all pkt in window" << endl;
    starttimer(0, TIME_OUT);
    for (int i = 0; i < window_buffer.size(); i++) {
        A.next_seq = (head + i) % A.WINDOW_SIZE;
        A.next_ack = A.next_seq;
        struct msg resend_msg;
        memcpy(resend_msg.data, window_buffer[i].data, 20);
        A.sender_state = SENDING_BUFFER;
        A_output(resend_msg);
        A.sender_state = WAITING_ACK;
    }
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init() {
    A.sender_state = WAITING_MSG;
    A.next_ack = 0;
    A.next_seq = 0;
    head = A.next_seq;
    tail = head;
    A.WINDOW_SIZE = getwinsize();
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet) {
    int checksum = get_checksum(&packet);
    if (checksum == packet.checksum) {
        cout << "Receive: " << packet.payload << ", seq=" << packet.seqnum << " ack=" << packet.acknum << endl;
        if (packet.seqnum == B.next_seq) {
            //case 1: received expected pkt--deliver pkt and send ack, then update seq and ack
            tolayer5(1, packet.payload);
            B.ack_pkt.acknum = packet.seqnum;
            B.ack_pkt.checksum = get_checksum(&B.ack_pkt);
            tolayer3(1, B.ack_pkt);
            B.next_seq = next_num(B.next_seq, A.WINDOW_SIZE);
            cout << "Response to sender: " << "ack=" << B.ack_pkt.acknum << " next expected seq=" << B.next_seq << endl;
        } else {
            //case 2: received unexpected pkt--send last ack
            B.ack_pkt.acknum = packet.seqnum;
            B.ack_pkt.checksum = get_checksum(&B.ack_pkt);
            cout << "Response to sender: " << "ack=" << B.ack_pkt.acknum << " next expected seq=" << B.next_seq << endl;
            tolayer3(1, B.ack_pkt);
        }
    } else {
        cout << "Broken packet" << endl;
    }
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init() {
    B.next_seq = 0;
    B.ack_pkt.seqnum = 0;
    memset(B.ack_pkt.payload, 0, 20);
}
