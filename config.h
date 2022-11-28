#ifndef _CONFIG_H
#define _CONFIG_H

#include <limits.h>

#define MAX_PROCESSES 40
#define HELP_STATEMENT "HELP\n"
#define MAX_PROCESS_IN_SYSTEM 18
#define MAX_LINE_IN_LOG_FILE 10000
#define TERMINATION_PROB 0.30
#define MAX_NANO_SEC 1000000000
#define NANO_NANO_SEC_PER_MILLISEC MAX_NANO_SEC/1000

#define MAX_PAGES 32
#define MAX_MEM 250
#define logSize 100000

const int DEFAULT_SS = 5;
const int MAX_PROCESS_GEN_SEC = 0;
const int MAX_PROCESS_GEN_MSEC =500;
const int MAX_PROCESS_TERMINATION_MSEC = 250;
const int TRUE =1;
const int FALSE =0;
const int CLOCK_INC_SEC = 1;
const int MAX_CLOCK_INC_NSEC = 1000;

enum request_state {request,release,none};
enum recvType {WAIT,NOWAIT};
enum mem_state {USED,FREE};

struct sembuf semLock, semRelease;

struct lclock{
    unsigned int sec;
    unsigned long long int nsec;
};

struct request_packet{
    int packet_id;
    unsigned int pte[MAX_PAGES];
    enum request_state type[MAX_PAGES];
};

struct send_pkt{
    long int mtype;
    int req_pkt_id;
};

struct recv_pkt{
    long int mtype;
    int status;
};

struct page_table_entry {
    int frame_no;
    int ref:1;
    int dirty:1;
    int present:1;
};

struct page_table{
    int pid;
    struct page_table_entry pte[MAX_PAGES];
};

key_t lc_key = 1234;
key_t sem_clock_key = 1545;
key_t page_table_key_st = 1645; // 1645 + 20 = 1665
key_t msgQ_key = 1745;
key_t request_packet_key_st = 4300; // 4300 + 18 = 4318
key_t request_packet_msgQ_key = 4321; // 4323
key_t recv_msgQ_key_st = 4323; // 4323 + 18 = 4341
key_t sem_process_key_st = 4423; // 4423 + 28 = 4461
key_t max_required_table_key = 5000;

#endif
