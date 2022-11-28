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
#include "globals.h"

// globals
int lc_sid;
struct lclock *lc_ptr;
int sem_clock;

int ptid[MAX_PROCESS_IN_SYSTEM];
struct page_table *ptset[MAX_PROCESS_IN_SYSTEM];

int req_pkt_set_id[MAX_PROCESS_IN_SYSTEM];
struct request_packet * req_pkt_set [MAX_PROCESS_IN_SYSTEM];

int process_page_map[MAX_PROCESS_IN_SYSTEM];
enum mem_state bit_mem_map[MAX_MEM];
int bit_vector[MAX_PROCESS_IN_SYSTEM];

int sendQ_id;
int recvQ_id_set[MAX_PROCESS_IN_SYSTEM];
int sem_process_id [MAX_PROCESS_IN_SYSTEM];
int childPids[MAX_PROCESSES];

int verberose = 1;
int logCounter=0;
int t_s;

char logEntry[logSize];
int createNewProcess;
int keepRunning;
int process_itr;
int process_in_systm;

int processes_terminated;
// queue
struct node {
    int pid;
    void * next;
};

struct node * wait_q;

void push(struct node ** head , int pid){

    struct node * new_node = (struct node *) malloc(sizeof(struct node));
    (new_node)->pid = pid;
    (new_node)->next = NULL;
    if(*head==NULL)
        *head  = new_node;
    else{
        struct node * init = *head;
        while(init->next!=NULL){
            init=(init)->next;
        }
        init->next = new_node;
    }
}

int pop(struct node ** head){
    struct node * top = (*head);
    *head=(*head)->next;
    int pid = top->pid;
    free(top);
    return pid;
}

bool isEmpty(struct node ** head){
    if(*head==NULL)
        return true;
    return false;
}

void printQ(struct node ** head){
    struct node * qptr = *head;

    while(qptr!=NULL){
        printf("%d\t",qptr->pid);
        qptr=qptr->next;
    }
    printf("\n");
}

//------------------------------------------------------------------

float getProb(){
    return (1.0*(rand()%100))/100.0;
}

unsigned int getRandom(int max,int min){
    if(max==min) return min;
    return (unsigned int) rand()% (max-min+1) + min;
}

//------------------------------------------------------------------
// random time
struct lclock getRandomTime(unsigned int maxs , unsigned int maxns){
    struct lclock t;
    t.sec = lc_ptr->sec + getRandom(maxs,0);
    t.nsec = lc_ptr->nsec + getRandom(maxns,0);
    if(t.nsec >= MAX_NANO_SEC)
    {
        t.sec += t.nsec/MAX_NANO_SEC;
        t.nsec = t.nsec%MAX_NANO_SEC;
    }
    return t;
}

struct lclock updateTime(int millisec){

    long long int nsec = millisec*NANO_NANO_SEC_PER_MILLISEC;
    struct lclock c;
    semop(sem_clock,&semLock,1);

    lc_ptr->nsec += nsec;

    if(lc_ptr->nsec >= MAX_NANO_SEC)
    {
        lc_ptr->sec += lc_ptr->nsec/MAX_NANO_SEC;
        lc_ptr->nsec = lc_ptr->nsec%MAX_NANO_SEC;
    }
    c.sec = lc_ptr->sec;
    c.nsec = lc_ptr->nsec;
    semop(sem_clock,&semRelease,1);
    return c;
}

struct lclock getTime(){

    struct lclock c;
    semop(sem_clock,&semLock,1);
    c.sec = lc_ptr->sec;
    c.nsec = lc_ptr->nsec;
    semop(sem_clock,&semRelease,1);
    return c;
}

void updateProcessCreationTime(){
    int msec = getRandom(MAX_PROCESS_GEN_MSEC,1);
    updateTime(msec);
}

struct lclock updateTerminationTime(){
    int msec = getRandom(MAX_PROCESS_TERMINATION_MSEC,0);
    return updateTime(msec);
}

// returns in millisec
double getTimeDiff(struct lclock a,struct lclock b){
    double diff=0.0;
    diff = (a.sec - b.sec)*1000; //convert to millisec
    diff += (1.0*(a.nsec>b.nsec?(a.nsec-b.nsec):(b.nsec-a.nsec))/MAX_NANO_SEC)*1000; // convert to mllisec
    return diff;
}

int compareLclock(struct lclock t){
    if(t.sec < lc_ptr->sec)
        return 1;
    else if(t.sec == lc_ptr->sec && t.nsec < lc_ptr->nsec)
        return 1;
    else if(t.sec == lc_ptr->sec && t.nsec == lc_ptr->nsec)
        return 1;
    else
        return 0;
}


void printClock(struct lclock s){

    printf("%d:%lld\n",s.sec,s.nsec);
}

// clear the shared mem before exit
void clear_oss_shared_mem(){


    msgctl(sendQ_id,IPC_RMID,NULL);

    for(int i=0;i<MAX_PROCESS_IN_SYSTEM;i++){
        msgctl(recvQ_id_set[i],IPC_RMID,NULL);
    }

    // detach the clock
    shmdt(lc_ptr);
    shmctl(lc_sid,IPC_RMID,NULL);

    // remove clock semaphore
    semctl(sem_clock,0,IPC_RMID);

    // remove the semaphores
    for(int i=0;i<MAX_PROCESS_IN_SYSTEM;i++){
        semctl(sem_process_id[i],0,IPC_RMID);
    }

    // remove child processes
    for(int i=0;i<process_in_systm;i++){
        kill (childPids[i], SIGINT);
    }
}

// SIGINT signal handle master
void signalHandle(){
    printf("OSS:Handled the SIGINT signal \n");
    clear_oss_shared_mem();
    exit(0);
}

// SIGTERM signal handle master
void signalHandleterminate(){
    printf("OSS: Handled the SIGTERM signal\n");
    clear_oss_shared_mem();
    exit(0);
}

// SIGALARM signal handle for alarm
void signalHandleAlarm(){
    printf("OSS: Handled the SIGALRM signal\n");
    createNewProcess = FALSE;
}

// SIGUSR1 signal handle for alarm
void signalHandleUsr1(){
    // do nothing
}


void logMaster(){
    FILE * fptr;
    char logfile [] = "logfile.txt";

    fptr = fopen(logfile,"a");
    struct lclock l = getTime();

    fprintf(fptr,"%d::%lld\t:\t%s\n",l.sec,l.nsec,logEntry);
    bzero(logEntry,logSize);
    fclose(fptr);
}


// bit vector
int checkFreePagetable(){
    for(int i=0;i<MAX_PROCESS_IN_SYSTEM;i++){
        if(bit_vector[i]==0)
            return i;
    }
    return -1;
}

void setpblock(int id){
    bit_vector[id]=1;
}

void freepblock(int id){
    bit_vector[id]=0;
}


// bit vector
int checkFreeMemory(){
    for(int i=0;i<MAX_MEM;i++){
        if(bit_mem_map[i]==FREE)
            return i;
    }
    return -1;
}

void setMemory(int id){
    bit_mem_map[id]=USED;
}

void freeMemory(int id){
    bit_mem_map[id]=FREE;
}


void printfVector(unsigned int *t,char *s,int size){
    printf("%s\n",s);
    for(int i=0;i<size;i++){
            printf("%d\t",t[i]);
    }
        printf("\n");
}

// process requests
int process_request(struct request_packet * p){

    // printf("Request Type%d : child %d\n",p->type[0],p->packet_id);
    // printfVector(&(available_count[0]),"available count",MAX_RESOURCE_COUNT);
    // printfVector(&(p->resource_instance_count[0]),"instance count",MAX_RESOURCE_COUNT);

    return 0;
}

struct request_packet * read_packet(enum recvType type){
    int status;
    struct send_pkt p;

    status = msgrcv(sendQ_id,&p,sizeof(p.req_pkt_id),1,IPC_NOWAIT);
    if(status==-1)
        return NULL;
    return req_pkt_set[p.req_pkt_id];
}


void write_pkt(int status,int id){

 }


void handleQueue(){
    while(isEmpty(&wait_q)!=TRUE){
        int pid = wait_q->pid;
        int status = process_request(req_pkt_set[pid]);
        if(status==1){
            semop(sem_process_id[pid],&semRelease,1);
            // printf("Request Type:%d , CHILD:%d , status:%d\n",req_pkt_set[pid]->type[0],req_pkt_set[pid]->packet_id,status);
            write_pkt(status,pid);
            pop(&wait_q);
        }
        else{
            break;
        }
    }
}


// child process
void childProcess(int req_pkt_id){


    exit(EXIT_SUCCESS);
}

int setNewProcess(){
    if(createNewProcess==TRUE && process_itr<MAX_PROCESSES){

        return -2;
    }
    return -3;
}

int main(int argc, char ** argv){

    signal(SIGINT,signalHandle);
    signal(SIGTERM,signalHandleterminate);
    signal(SIGUSR1,signalHandleUsr1);
    signal(SIGALRM, signalHandleAlarm);
    signal(SIGCHLD,signalHandleUsr1);

    // RESET SEM
    semLock.sem_num=0;
    semLock.sem_op=-1;
    semLock.sem_flg=0;

    semRelease.sem_num=0;
    semRelease.sem_op=1;
    semRelease.sem_flg=0;

    // default values
    t_s = DEFAULT_SS;

    // decode the inputs
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"-h")==0){
            printf(HELP_STATEMENT);
            return 0;
        }
        else if(strcmp(argv[i],"-v")==0){
            verberose = atoi(argv[i+1]);
            i++;
        }
    }

    // initialize the clock
    lc_sid = shmget(lc_key,sizeof(struct lclock),IPC_CREAT|0666);
    if(lc_sid==-1 ){
        perror("Unable to create clock memory");
        exit(0);
    }
    lc_ptr = (struct lclock*)shmat(lc_sid,NULL,0);
    if(lc_ptr==(struct lclock *)-1 ){
        perror("Unable to attach clock struct");
        exit(0);
    }

    lc_ptr->sec = 0;
    lc_ptr->nsec = 0;

    // initialise clock sempaphore
    sem_clock = semget(sem_clock_key,1,0666|IPC_CREAT);
    if(sem_clock==-1){
        perror("semget clock error");
        exit(1);
    }

    int status = semctl(sem_clock,0,SETVAL,1);
    if(status == -1){
        perror("semctl clock error");
        exit(1);
    }

    // set the alarm
    snprintf(logEntry,1000,"MASTER: started with t_s as %d\n",t_s);
    logMaster();

    createNewProcess = TRUE;
//    alarm(t_s);

    // initialize the memory
    for(int i=0;i,MAX_MEM;i++)
        bit_mem_map[i] =  FREE;

    // initialise the page tables
    int page_table_size = sizeof(struct page_table);

    for(int i=0;i<MAX_PROCESS_IN_SYSTEM;i++){
        ptid[i] = shmget((page_table_key_st+i),page_table_size,0666|IPC_CREAT);
        if(ptid[i] == -1){
            perror("Unable to get memory for resource\n");
            clear_oss_shared_mem();
            exit(0);
        }
        ptset[i] = (struct page_table *)shmat(ptid[i],(void*)0,0);
        if(ptset[i] == (struct page_table *)-1){
            perror("Unable to attach to memory for resource\n");
            clear_oss_shared_mem();
            exit(0);
        }
        float isSharedRes =  getProb();

        // initialise the resource descriptors
        ptset[i]->pid=i+1;
        for(int j=0;j<MAX_PAGES;j++){
            ptset[i]->pte[j].frame_no=-1;
            ptset[i]->pte[j].present=0;
            ptset[i]->pte[j].dirty=0;
            ptset[i]->pte[j].ref=0;
        }
    }

    //initialise request Set
    int request_pkt_size = sizeof(struct request_packet);

    for(int i=0;i<MAX_PROCESS_IN_SYSTEM;i++){

        req_pkt_set_id[i] = shmget((request_packet_key_st+i),request_pkt_size,0666|IPC_CREAT);
        if(req_pkt_set_id[i] == -1){
            perror("Unable to get memory for request packet\n");
            clear_oss_shared_mem();
            exit(0);
        }
        req_pkt_set[i] = (struct request_packet *)shmat(req_pkt_set_id[i],(void*)0,0);
        if(req_pkt_set[i] == (struct request_packet*)-1){
            perror("Unable to attach to memory for request packet\n");
            clear_oss_shared_mem();
            exit(0);
        }

        req_pkt_set[i]->packet_id = i;
        for(int j=0;j<MAX_PAGES;j++){
            req_pkt_set[i]->pte[j]=0;
            req_pkt_set[i]->type[j] = none;
        }
    }

    //initialise request Set

    // msg queues initialization
    sendQ_id = msgget(request_packet_msgQ_key,IPC_CREAT|0666);
    for(int i=0;i<MAX_PROCESS_IN_SYSTEM;i++)
        recvQ_id_set[i] = msgget((recv_msgQ_key_st+i),IPC_CREAT|0666);

    // initialize the sem for processes
    for(int i=0;i<MAX_PROCESS_IN_SYSTEM;i++){
        sem_process_id[i] = semget((sem_process_key_st+i),1,0666|IPC_CREAT);
        if(sem_process_id[i]==-1){
            perror("semget process error");
            clear_oss_shared_mem();
            exit(1);
        }

        int status = semctl(sem_process_id[i],0,SETVAL,1);
        if(status == -1){
            perror("semctl process error");
            clear_oss_shared_mem();
            exit(1);
        }
    }

    // while termination not achieved

    keepRunning=TRUE;
    process_itr=0;
    process_in_systm=0;
    processes_terminated =0;

    while(keepRunning){

        // create the processes
        status = -2;
        if(process_in_systm==0 && process_itr==MAX_PROCESSES){
            keepRunning=FALSE;
            break;
        }

        if(process_in_systm <=MAX_PROCESS_IN_SYSTEM){
            status = setNewProcess();
            if(status == 1){
                process_in_systm++;
            }
            else if(status==-2 ){
                // print the error
  //              printf("system full with process!!\n");
            }
            else if(status==-3 ){
                // print the error
//                printf("Totoal processes created\n");
            }
        }
        handleQueue();
        // if max process reached
        struct request_packet *p=NULL;

        if(status < 1){
            // check for any new request with wait
           p =  read_packet(WAIT);
        }
        else{
            // check for any new request without wait
           p =  read_packet(NOWAIT);
        }

        if(p!=NULL){
            status = process_request(p);
            // printf("Request Type:%d , CHILD:%d , status:%d\n",p->type[0],p->packet_id,status);
            write_pkt(status, p->packet_id);
        }
    }
    clear_oss_shared_mem();
    return 0;
}
