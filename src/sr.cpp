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

queue<string> waiting_queue;
vector<string> window_buffer;

int baseA, baseB, N, head, tail;


struct MsgCache {
    struct msg* m;
    int acked;
    int time;
};

struct MsgCache* BufMsg[1005];
struct msg* BufData[1005];

int next_num(int num, int N){
    return (num + 1) % N;
}

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
    SENDING_BUFFER,
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

int min(int a,int b){
    return a<b?a:b;
}

static pkt* make_packet(int seq, int ack, char* data){
    struct pkt* packet = new pkt();
    packet->seqnum = seq;
    packet->acknum = ack;
    strncpy(packet->payload, data, 20);
    packet->checksum = get_checksum(packet);
    return packet;
}

static void send_packet(int type, struct pkt *packet){
    if (type == 0){
        cout << "[A - send]: " << packet->payload << ", seq=" << packet->seqnum << " ack=" << packet->acknum << endl;
    }
    else{
        cout << "[B - ACK]: " << baseB << " | " << packet->payload << ", seq = " << packet->seqnum << ", ack= " <<packet->acknum << endl;
    }
    tolayer3(type, *packet);
}

/* called from layer 5, passed the data to be sent to other side */
void A_output(struct msg message) {
    if (A.sender_state == WAITING_ACK) {
        waiting_queue.push(string(message.data));
        return;
    }
    //add pkt info: get seqnum, acknum, compute checksum, copy msg to payload

    if (A.sender_state == WAITING_MSG) {
        if (window_buffer.empty()) {
            starttimer(0, TIME_OUT);
            tail = head;
        } else {
            if (window_buffer.size() == N - 1) {
                A.sender_state = WAITING_ACK;
            } else {
                cout << "Window not full, send msg" << endl;
            }
            tail = next_num(tail, N);
        }
        window_buffer.push_back(string(message.data));
    }


    struct msg* m = new msg();
    struct MsgCache* mc = new MsgCache();
    strncpy(m->data, message.data, 20);
    mc->m = m;
    mc->acked = 0;
    mc->time = get_sim_time();
    BufMsg[A.next_seq] = mc;
    cout << "[A] get msg!" << endl;

    if (A.next_seq < baseA + N){
        //A.next_ack = A.next_seq;
        //A.next_packet.seqnum = A.next_seq;
        //A.next_packet.acknum = A.next_ack;
        //memcpy(A.next_packet.payload, message.data, sizeof(message.data));
        //int ckecksum = get_checksum(&A.next_packet);
        //A.next_packet.checksum = ckecksum;
        //then send pkt to layer 3 using tolayer3()
        //cout << "Send: " << message.data << ", seq=" << A.next_seq << " ack=" << A.next_ack << endl;
        //tolayer3(0, A.next_packet);
        struct pkt* packet = make_packet(A.next_seq, A.next_ack, BufMsg[A.next_seq]->m->data);
        send_packet(0, packet);
    }
        //notice that timer need to be operate
    //A.next_seq = next_num(A.next_seq, N);
    //A.next_ack = A.next_seq;

}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet) {
    if (A.sender_state != WAITING_ACK) {
        return;
    }
    if (packet.checksum != get_checksum(&packet)) {
        cout << "[A] bad packet!" << endl;
        return;
    }
    if (packet.acknum < baseA || packet.acknum > min(baseA + N, A.next_seq)){
        cout << "[A] out of order!" << endl;
        return;
    }
    BufMsg[packet.acknum]->acked = 1;
    if (packet.acknum == baseA){
        baseA = packet.acknum + 1;
    }
    A.next_ack = packet.seqnum;
    cout << "[A - getACK] " << packet.payload << "seq = " << packet.seqnum << "ack = " << packet.acknum << endl;
}

/* called when A's timer goes off */
void A_timerinterrupt() {
    if (A.sender_state != WAITING_ACK) {
        return;
    }
    int curtime = get_sim_time();
    for (int i = baseA; i < min(baseA + N, A.next_seq); i++){
        if(BufMsg[i]->acked == 0 && BufMsg[i]->time + TIME_OUT < curtime){
            A.next_seq = (head + i) % N;
            A.next_ack = A.next_seq;
            struct msg resend_msg;
            memcpy(resend_msg.data, (char *) window_buffer[i].data(), 20);
            A.sender_state = SENDING_BUFFER;
            A_output(resend_msg);
            A.sender_state = WAITING_ACK;
            BufMsg[i]->time = curtime;
            //printf("[A - resend]");
            //struct pkt* packet = make_packet(i, A.next_ack, BufMsg[i]->m->data);
            //send_packet(0, packet);
            //A.sender_state = WAITING_ACK;
            //BufMsg[i]->time = curtime;
        }
    }
    starttimer(0, 10);
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init() {
    //some initialization of global parameters, like seqnum, acknum
    A.sender_state = WAITING_MSG;
    A.next_ack = 0;
    A.next_seq = 0;
    head = A.next_seq;
    tail = head;
    baseA = 1;
    N = getwinsize();
    starttimer(0, 10);
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet) {
    int checksum = get_checksum(&packet);
    if (packet.checksum != checksum){
        cout << "[B] bad packet" << endl;
        return;
    }
    if (packet.seqnum < baseB - N || packet.seqnum > baseB + N){
        cout << "[B] " << packet.seqnum << ", " << baseB << " out of order" << endl;
        return;
    }
    struct msg* m = new msg();
    strncpy(m->data, packet.payload, 20);
    BufData[packet.seqnum] = m;
    char message[20];

    if (baseB == packet.seqnum){
        while(BufData[baseB] != NULL){
            strncpy(message, BufData[baseB]->data, 20);
            cout << "[B - in] " << baseB << ", " << BufData[baseB] << ", "
                << "seq= " << packet.seqnum << "ack= " << packet.acknum << endl;
            tolayer5(1, message);
            baseB++;
        }
    }

    cout << "Receive: " << packet.payload << ", seq=" << packet.seqnum << " ack=" << packet.acknum << endl;
    if (packet.seqnum == B.next_seq) {
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
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init() {
    //some initialization of global parameters
    memset(BufData, 0, sizeof(BufData));
    baseB = 1;
    B.next_seq = 0;
    B.ack_pkt.seqnum = 0;
    memset(B.ack_pkt.payload, 0, 20);
}
