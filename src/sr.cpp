#include "../include/simulator.h"
#include <stdio.h>
#include <string.h>
#include <queue>
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

/* called from layer 5, passed the data to be sent to other side */

#define TIME_OUT 20.0f

struct pkt_time{
  struct pkt packet;
  float start_time;
};

int N, baseB;
queue<pkt> wait_queue;
vector<pkt_time> A_windows;
vector<pkt> B_windows;

struct SenderInfo {
    int next_seq;
    int next_ack;
    int base;
    int last_rec_seq;
    struct pkt next_packet;
} A;

/* calculate the checksum of the package  */
int get_checksum(struct pkt *packet) {
    int cs = 0;
    cs += packet->seqnum;
    cs += packet->acknum;
    for (int i = 0; i < 20; i++) {
        cs += packet->payload[i];
    }
    return cs;
}

void A_output(struct msg message)
{
  A.next_packet.seqnum = sequence;
  A.next_packet.acknum = 0;
  bzero(&A.next_packet.payload,sizeof(A.next_packet.payload));
  strncpy(A.next_packet.payload,message.data,sizeof(A.next_packet.payload));
  A.next_packet.checksum = get_checksum(&A.next_packet);

  float start_time = get_sim_time();
  if(A_windows.size() == 0) {
    starttimer(0,TIME_OUT);
  }
  if(A.next_seq - A.base < N){
    if(wait_queue.empty()){
      tolayer3(0,A.next_packet);
      struct pkt_time pkt_t;
      pkt_t.packet = A.next_packet;
      pkt_t.start_time = start_time;
      A_windows.push_back(pkt_t);
    }
    else{
      while(!wait_queue.empty() && wait_queue.front().seqnum - A.base < N){
        struct pkt p = wait_queue.front();
        wait_queue.pop();
        struct pkt_time pkt_t;
        pkt_t.packet = A.next_packet;
        pkt_t.start_time = start_time;
        A_windows.push_back(pkt_t);
        tolayer3(0,p);
      }
      if(A.next_seq - A.base < N){
        struct pkt_time pkt_t;
        pkt_t.packet = A.next_packet;
        pkt_t.start_time = start_time;
        A_windows.push_back(pkt_t);
        tolayer3(0,A.next_packet);
      }
      else{
        wait_queue.push(A.next_packet);
      }
    }
  }
  else{
    wait_queue.push(A.next_packet);
  }
  A.next_seq++;
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(struct pkt packet)
{
  if(packet.checksum == get_checksum(&packet)){
    for(vector<pkt_time>::iterator iter = A_windows.begin();iter != A_windows.end();++iter){
      if(iter->packet.seqnum == packet.seqnum){
        A_windows.erase(iter);
        break;
      }
    }
    A.last_rec_seq = max(A.last_rec_seq,packet.seqnum);
    if(packet.seqnum == A.base){
      stoptimer(0);
      float cur_time = get_sim_time();
      int min_seq = 2000;
      int max_seq = 0;
      if(A_windows.size() == 0){
        A.base = A.last_rec_seq + 1;
        max_seq = A.last_rec_seq;
      } else {
        for(vector<pkt_time>::iterator iter = A_windows.begin();iter != A_windows.end();++iter){
          min_seq = min(min_seq,iter->packet.seqnum);
          max_seq = max(max_seq,iter->packet.seqnum);
        }
        A.base = min_seq;
        starttimer(0,TIME_OUT - (cur_time - A_windows[0].start_time)); 
      }

      while(!wait_queue.empty() && max_seq - A.base + 1 < N){
        if(A_windows.size() == 0){
          starttimer(0,TIME_OUT);
        }
        struct pkt p = wait_queue.front();
        wait_queue.pop();
        float start_time = get_sim_time();
        struct pkt_time pkt_t;
        pkt_t.packet = p;
        pkt_t.start_time = start_time;
        A_windows.push_back(pkt_t);
        tolayer3(0,p);
        max_seq = p.seqnum;
      }
    }
  }
}

/* called when A's timer goes off */
void A_timerinterrupt()
{
  float cur_time = get_sim_time();
  pkt_time head = A_windows[0];
  head.start_time = cur_time;
  A_windows.push_back(head);
  A_windows.erase(A_windows.begin());
  tolayer3(0,head.packet);
  starttimer(0,TIME_OUT - (cur_time - A_windows[0].start_time));
}  

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init()
{
  N = getwinsize();
  A.last_rec_seq = 0;
  A.base = 1;
  A.next_seq = 1;
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */


/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet)
{
  if( packet.checksum == get_checksum(&packet)){
    struct pkt ack;
    char message[20];
    struct pkt newp;
    newp.acknum = 0;
    ack.seqnum = packet.seqnum;
    ack.acknum = 0;
    strcpy(ack.payload,packet.payload);
    ack.checksum = get_checksum(&ack);
    tolayer3(1,ack);
    if(packet.seqnum == baseB){
      packet.acknum = 1;
      B_windows[0] = packet;
      while(!B_windows.empty() && B_windows[0].acknum == 1){
        strncpy(message, B_windows[0].payload, 20);
        B_windows.erase(B_windows.begin());
        B_windows.push_back(newp);
        tolayer5(1, message);
        baseB++;
      }
    }
    else if(packet.seqnum > baseB && packet.seqnum < baseB + N){
      packet.acknum = 1;
      B_windows[packet.seqnum - baseB] = packet;
    }
  }
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init()
{
  baseB = 1;
  for(int i = 0;i != N;++i){
    struct pkt p;
    B_windows.push_back(p);
  }
}
