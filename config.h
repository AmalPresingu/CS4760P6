#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <limits.h>

#define MAX_PROCESSES 20
#define HELP_STATEMENT "USAGE\n---------\n./oss                   Runs with default processes\n./oss -h                Prints usage\n./oss -m 0              Runs 1st memory request scheme\n./oss -m 1              Runs 2nd memory request scheme\n./oss -p [n]            Set number of processes n\n"
#define MAX_PROCESS_IN_SYSTEM 18
#define MAX_LINE_IN_LOG_FILE 40000
#define TERMINATION_PROB 0.30
#define MAX_NANO_SEC 1000000000
#define NANO_NANO_SEC_PER_MILLISEC MAX_NANO_SEC/1000
#define MEM_ALLOCATION_TIME 10 // in nanosec
#define PAGE_FAULT_RUNTIME 14000000 // in nanosec
#define MAX_PAGES 32
#define MAX_MEM 256
#define logSize 100000

const int DEFAULT_SS = 10;
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
enum req_type {READ,WRITE,TERMINATE};
enum pnp {PRESENT,NOTPRESENT};

struct sembuf semLock, semRelease;

struct lclock{
    unsigned int sec;
    unsigned long long int nsec;
};

struct request_packet{
    int packet_id;
    int page_ref;
    int offset;
    enum req_type t;
    struct lclock req_time;
    struct lclock res_time;
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
    int ref;
    int dirty;
    int present;
    int valid;
    int size;
};

struct page_table{
    int pid;
    struct page_table_entry pte[MAX_PAGES];
};

struct stat {
    double mem_access_count;
    double speed;
    int page_faults;
    int seg_faults;
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
