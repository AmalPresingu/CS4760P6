//Name   :   Amal Presingu
//Date   :   11/29/2022

#include "config.h"

// globals
int lc_sid;
struct lclock *lc_ptr;
int sem_clock;
int Max_User_Processes;
int Max_User_Process_in_system;
int page_selection_type;
int ptid[MAX_PROCESS_IN_SYSTEM];
int total_avialable_frame;
struct page_table *ptset[MAX_PROCESS_IN_SYSTEM];
int req_pkt_set_id[MAX_PROCESS_IN_SYSTEM];
struct request_packet * req_pkt_set [MAX_PROCESS_IN_SYSTEM];

struct stat stat_set;

int process_page_map[MAX_PROCESS_IN_SYSTEM];
enum mem_state bit_mem_map[MAX_MEM];
int bit_vector[MAX_PROCESS_IN_SYSTEM];

int sendQ_id;
int recvQ_id_set[MAX_PROCESS_IN_SYSTEM];
int sem_process_id [MAX_PROCESS_IN_SYSTEM];
int childPids[MAX_PROCESSES];

int verberose = 1;
int logCounter=0;
unsigned int t_s;

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

struct node * wait_q,*wait_response_q;

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

void emptyQueue(struct node ** head){
    while(!isEmpty(head)){
        pop(head);
    }
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
    float p =  (1.0*(rand()%100))/100.0;
    return p;
}

unsigned int getRandom(int max,int min){
    if(max==min) return min;
    return (unsigned int) (rand()% (max-min+1)) + min;
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

struct lclock updateTimeNano(int nanosec){

    struct lclock c;
    semop(sem_clock,&semLock,1);

    lc_ptr->nsec += nanosec;

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

struct lclock updateProcessCreationTime(){
    int msec = getRandom(MAX_PROCESS_GEN_MSEC,1);
    return updateTime(msec);
}

struct lclock updateTerminationTime(){
    int msec = getRandom(MAX_PROCESS_TERMINATION_MSEC,0);
    return updateTime(msec);
}

// returns in milliseconds
double getTimeDiff(struct lclock a,struct lclock b){
    double diff=0.0;
    diff = (a.sec - b.sec)*1000; //convert to milliseconds
    diff += (1.0*(a.nsec>b.nsec?(a.nsec-b.nsec):(b.nsec-a.nsec))/MAX_NANO_SEC)*1000; // convert to milliseconds
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

// clear the system before exit
void clear_oss_shared_mem(){

    // remove child processes
    for(int i=0;i<process_in_systm;i++){
        kill (childPids[i], SIGUSR1);
    }

    // detach the page table blocks

    for(int i=0;i<Max_User_Process_in_system;i++){
        shmdt(ptset[i]);
        shmctl(ptid[i],IPC_RMID,NULL);
    }

    // detach the requests blocks
    for(int i=0;i<Max_User_Process_in_system;i++){
        shmdt(req_pkt_set[i]);
        shmctl(req_pkt_set_id[i],IPC_RMID,NULL);
    }

    // detach the msg queues
    msgctl(sendQ_id,IPC_RMID,NULL);

    for(int i=0;i<Max_User_Process_in_system;i++){
        msgctl(recvQ_id_set[i],IPC_RMID,NULL);
    }

    // detach the clock
    shmdt(lc_ptr);
    shmctl(lc_sid,IPC_RMID,NULL);

    // remove clock semaphore
    semctl(sem_clock,0,IPC_RMID);

    // remove the semaphores
    for(int i=0;i<Max_User_Process_in_system;i++){
        semctl(sem_process_id[i],0,IPC_RMID);
    }

    // empty queue
    emptyQueue(&wait_q);
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
    clear_oss_shared_mem();
    exit(0);
}

// SIGUSR1 signal handle for alarm
void signalHandleUsr1(){
    // do nothing
}

// SIGUSR1 signal handle for alarm
void signalHandleUsrChild(){
    exit(0);
}

// log master
int globalLogCount=0;
void logMaster(){

    if(globalLogCount < MAX_LINE_IN_LOG_FILE){
        FILE * fptr;
        char logfile [] = "logfile.txt";

        fptr = fopen(logfile,"a");
        struct lclock l = getTime();

        fprintf(fptr,"%d::%lld\t:\t%s",l.sec,l.nsec,logEntry);
        bzero(logEntry,logSize);
        fclose(fptr);
        globalLogCount++;
    }
}


// bit vector
int checkFreePagetable(){
    for(int i=0;i<Max_User_Process_in_system;i++){
        if(bit_vector[i]==0)
            return i;
    }
    return -1;
}

void setpageTable(int id){
    bit_vector[id]=1;
}

void freepageTable(int id){
    bit_vector[id]=0;
}


// bit vector for memory
int checkFreeMemory(){
    for(int i=0;i<MAX_MEM;i++){
        if(bit_mem_map[i]==FREE)
            return i;
    }
    return -1;
}

void setMemory(int id){
    bit_mem_map[id]=USED;
    total_avialable_frame--;
}

void freeMemory(int id){
    bit_mem_map[id]=FREE;
    total_avialable_frame++;
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
    stat_set.mem_access_count++;

    // detecting segmentation fault
    if(p->page_ref>31){
        stat_set.seg_faults++;
        return -2;
    }

    //  terminated   process
    if(p->t==TERMINATE){
        return 2;
    }

    long int address  = p->page_ref<<10;
    address += p->offset;
    if(p->t == READ)
        snprintf(logEntry,1000,"MASTER- Process %d Read Request for address %ld\n",process_page_map[p->packet_id]+1,address);
    else
        snprintf(logEntry,1000,"MASTER- Process %d Write Request for address %ld\n",process_page_map[p->packet_id]+1,address);

    logMaster();

    struct page_table * pt = ptset[p->packet_id];

    // check if page has frame
    if(pt->pte[p->page_ref].present==PRESENT){
        pt->pte[p->page_ref].ref+=1;
        pt->pte[p->page_ref].valid=1;
        if(p->t==WRITE){
            pt->pte[p->page_ref].dirty=1;
        }
        updateTimeNano(10);
        snprintf(logEntry,1000,"MASTER- Process %d Request for address %ld: NO PAGE FAULT\n",process_page_map[p->packet_id]+1,address);
        logMaster();
    }
    else{
        int id = checkFreeMemory();
        stat_set.page_faults++;
        snprintf(logEntry,1000,"MASTER- Process %d Request for address %ld: PAGE FAULT\n",process_page_map[p->packet_id]+1,address);
        logMaster();
        // if free frame found
        if(id!=-1){

            // add disk read time
            updateTime(14);
            setMemory(id);
            pt->pte[p->page_ref].present=PRESENT;
            pt->pte[p->page_ref].frame_no=id+1;
            pt->pte[p->page_ref].valid=1;
            pt->pte[p->page_ref].ref=1;
            if(p->t==WRITE){
                pt->pte[p->page_ref].dirty=1;
            }

            for(int i=0;i<MAX_PAGES;i++){
                if(pt->pte[i].present==PRESENT && pt->pte[i].valid<0 ){
                    snprintf(logEntry,1000,"MASTER- Process %d FREES FRAME %d\n",process_page_map[p->packet_id]+1,pt->pte[i].frame_no);
                    logMaster();
                    freeMemory(pt->pte[i].frame_no);
                    pt->pte[i].present = NOTPRESENT;
                    pt->pte[i].frame_no=0;
                    pt->pte[i].valid=0;
                    pt->pte[i].ref=0;
                }
            }
            snprintf(logEntry,1000,"MASTER- Process %d Request for address %ld: PAGE FAULT : FREE FRAME AVAILABLE\n",process_page_map[p->packet_id]+1,address);
            logMaster();
        }
        // page replacement in case of no free frame
        else{
            // CLOCK ALGORITHM
            // get free frame
            // check for the frame with minimum reference
            // and replace the one which is first in the list
            // also reduce the value of reference
            // this way we can move in circle and replace the first with minimum reference
            int i=0,min_ref=INT_MAX,mrfid=-1;

            for(i=0;i<MAX_PAGES;i++){
                if(pt->pte[i].present==PRESENT){
                    pt->pte[i].ref-=1;
                    if(pt->pte[i].ref < min_ref){
                        min_ref=pt->pte[i].ref;
                        mrfid=i;
                    }
                }
            }

            if(mrfid==-1){
                return -1;
            }

            //lazy write
            if(pt->pte[mrfid].dirty==1){
                // add disk write time;
                    updateTime(14);
            }

            // set page
            // add frame read time
            updateTime(14);
            pt->pte[p->page_ref].present=PRESENT;
            pt->pte[p->page_ref].frame_no=pt->pte[mrfid].frame_no;
            pt->pte[p->page_ref].valid=1;
            pt->pte[p->page_ref].ref=1;
            if(p->t==WRITE){
                pt->pte[p->page_ref].dirty=1;
            }

            snprintf(logEntry,1000,"MASTER- Process %d Request for address  %ld: PAGE FAULT : NO FREE FRAME : PAGE %d SWAPPED FOR PAGE %d\n",process_page_map[p->packet_id]+1,address,mrfid,p->page_ref);
            logMaster();

            // release page
            pt->pte[mrfid].present=NOTPRESENT;
            pt->pte[mrfid].frame_no=0;
            pt->pte[mrfid].valid=0;
            pt->pte[mrfid].ref=0;
        }
    }


    return 1;
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
// success
    if(status == 1){
        // struct recv_pkt p;
        // p.mtype=1;
        // p.status = status;
        // msgsnd(recvQ_id_set[id],&p,sizeof(p.status),0);
        semop(sem_process_id[id],&semRelease,1);
    }
    // push in wait_q in make the process sleep
    else if(status  == -1){
        // struct recv_pkt p;
        // p.mtype=1;
        // p.status = status;
        // msgsnd(recvQ_id_set[id],&p,sizeof(p.status),0);
        push(&wait_q,id);
    }
    else if (status == 2){
        // terminated
        struct page_table * pt = ptset[id];
        for(int i=0;i<MAX_PAGES;i++){
            freeMemory(pt->pte[i].frame_no);
            pt->pte[i].present = NOTPRESENT;
            pt->pte[i].frame_no=0;
            pt->pte[i].valid=0;
            pt->pte[i].ref=0;
        }
        pt->pid=-1;

        processes_terminated++;
        process_in_systm--;
        freepageTable(id);
        // printf("OSS:%d process Terminated   \n",process_page_map[id]);
        snprintf(logEntry,1000,"MASTER- Process %d terminated \n",process_page_map[id]+1);
        logMaster();
        process_page_map[id]=-1;
    }
    // terminate the process
    else if(status==-2){
        // terminated
        kill (childPids[process_page_map[id]], SIGUSR1);
        // clean up
        struct page_table * pt = ptset[id];
        for(int i=0;i<MAX_PAGES;i++){
            freeMemory(pt->pte[i].frame_no);
            pt->pte[i].present = NOTPRESENT;
            pt->pte[i].frame_no=0;
            pt->pte[i].valid=0;
            pt->pte[i].ref=0;
        }
        pt->pid=-1;
        processes_terminated++;
        process_in_systm--;
        freepageTable(id);
        // printf("OSS:%d process Terminated   \n",process_page_map[id]);
        snprintf(logEntry,1000,"MASTER- Process %d terminated FOR SEGMENTATION FAULT\n",process_page_map[id]+1);
        logMaster();
        process_page_map[id]=-1;
    }
    else{
        perror("Unown status Error\n");
    }
 }


void handleQueue(){
    while(isEmpty(&wait_q)!=TRUE){
        int pid = wait_q->pid;
        int status = process_request(req_pkt_set[pid]);
        if(status==1){
            write_pkt(status,pid);
            pop(&wait_q);
        }
        else{
            break;
        }
    }
}

void generatePorbSet(double *a){
    a[0]=1;
    for(int i=1;i<MAX_PAGES;i++)
        a[i]=a[i-1]+(1.0/(i+1));
}

int getProbPage(float id,double*a){
    int i=0;
    for( ;i<MAX_PAGES;i++){
        if(a[i]>id)
            break;
    }
    return i;
}

void get_page(struct page_table * p){
    // (10% of 256) = 25
    if(total_avialable_frame<=25){
        // set lowest (5% of 32) =1 as invalid i.e. the lowest ref value is to be set to invalid

        int i=0,min_ref=INT_MAX,mrfid=-1;

        //  get first id with min ref.
        for(i=0;i<MAX_PAGES;i++){
            if(p->pte[i].present==PRESENT){
                if(p->pte[i].ref < min_ref){
                    min_ref=p->pte[i].ref;
                    mrfid=i;
                }
            }
        }
        // decrement the valid value; it will be handled by the oss
        // if valid value == 0 more than once it thats when oss frees the memory
        if(mrfid!=-1){
            p->pte[mrfid].valid-=1;
        }
    }
}

// child process
void childProcess(int req_pkt_id){

    signal(SIGUSR1,signalHandleUsrChild);
    double a[MAX_PAGES];
    generatePorbSet(&a[0]);
    int aMax= (int) (a[MAX_PAGES-1]+1);

    struct request_packet * p = (struct request_packet*) shmat(req_pkt_set_id[req_pkt_id],(void*)0,0);
    struct page_table * pt = (struct page_table *) shmat(ptid[req_pkt_id],(void*)0,0);
    int recvQ_id = recvQ_id_set[req_pkt_id];
    int sem_id = sem_process_id[req_pkt_id];
    int sendq_id = sendQ_id;
    struct send_pkt send_msg;
    struct recv_pkt recv_msg;

    recv_msg.mtype=1;
    send_msg.mtype=1;

    send_msg.req_pkt_id = req_pkt_id;
    int count_men_ref=0;
    int keepRunning=1;
    while(keepRunning){

        get_page(pt);

        int page_request;
        if(page_selection_type==0){
          page_request =getRandom(31,0);
        }
        else{
            int id = getRandom(aMax,0);
            page_request = getProbPage(id,&a[0]);
        }
        p->page_ref = page_request;
        p->offset = getRandom(1023,0);
        p->t = getRandom(1,0)==1?READ:WRITE;

        // very rare segmentation fault occurrence
        if(getProb()==(float)0.99)
            p->page_ref=getRandom(100,0);

        int status = msgsnd(sendq_id,&send_msg,sizeof(send_msg.req_pkt_id),0);
        if(status == -1){
           char errorMsg [100];
        //    snprintf(errorMsg,100,"cannot send msg to OSS CHILD:%d",req_pkt_id);
        //    perror(errorMsg);
           exit(EXIT_SUCCESS);
        }

        semop(sem_id,&semLock,1);
        count_men_ref++;

        // try for termination if counter threshold reached
        if(count_men_ref>10000){
            // reset the counter
            count_men_ref=0;
            // check for termination
            if(getProb()>0.5){
                keepRunning=0;
                break;
            }
        }
    }
    p->page_ref=0;
    p->t = TERMINATE;
    // shmdt
    int status = msgsnd(sendQ_id,&send_msg,sizeof(send_msg.req_pkt_id),0);
    exit(EXIT_SUCCESS);
}


struct lclock c;

int setNewProcess(){

    // if(checkFreeMemory()==-1)
    //     return -4;
    int id=-1;
    if(createNewProcess==TRUE && process_itr<Max_User_Processes){
        int free_page_table = checkFreePagetable();
        if(free_page_table!=-1){
            id = fork();
            if(id==0){
                int id = free_page_table;
                childProcess(id);
                exit(0);
            }
            else if(id==-1){
                perror("Cannot create new process");
                return -1;
            }
            else if(id>0){
                c = updateProcessCreationTime();
                setpageTable(free_page_table);
                ptset[free_page_table]->pid=process_itr;
                process_page_map[free_page_table] = process_itr;
                childPids[process_itr]=id;
                process_itr++;
                // printf("OSS:%d process created \n",process_itr);
                snprintf(logEntry,1000,"MASTER- Process %d created \n",process_itr);
                logMaster();
                return 1;
            }
        }
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
    globalLogCount=0;
    Max_User_Process_in_system=MAX_PROCESS_IN_SYSTEM;
    Max_User_Processes=MAX_PROCESSES;
    // decode the inputs
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"-h")==0){
            printf(HELP_STATEMENT);
            return 0;
        }
        else if(strcmp(argv[i],"-m")==0){
            page_selection_type = atoi(argv[i+1]);
            i++;
        }
        else if(strcmp(argv[i],"-p")==0){
            Max_User_Processes = atoi(argv[i+1]);
            if(Max_User_Processes > MAX_PROCESSES)
                Max_User_Processes=MAX_PROCESSES;
            i++;
            Max_User_Process_in_system = Max_User_Processes;
            if(Max_User_Process_in_system > MAX_PROCESS_IN_SYSTEM)
                Max_User_Process_in_system = MAX_PROCESS_IN_SYSTEM;
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

    // initialize clock sempaphore
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
    snprintf(logEntry,1000,"MASTER: started with alarm set at %u\n",t_s);
    logMaster();

    createNewProcess = TRUE;
    alarm(t_s);

    stat_set.mem_access_count=0;
    stat_set.page_faults=0;
    stat_set.seg_faults=0;
    stat_set.speed=0;

    // initialize the memory
    for(int i=0;i<MAX_MEM;i++)
        bit_mem_map[i] =  FREE;

    // initialize the page tables
    int page_table_size = sizeof(struct page_table);

    for(int i=0;i<Max_User_Process_in_system;i++){
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
        // initialize the resource descriptors
        ptset[i]->pid=i+1;
        for(int j=0;j<MAX_PAGES;j++){
            ptset[i]->pte[j].frame_no=0;
            ptset[i]->pte[j].present=NOTPRESENT;
            ptset[i]->pte[j].dirty=0;
            ptset[i]->pte[j].ref=0;
            ptset[i]->pte[j].valid=0;
        }
    }

    //initialize request Set
    int request_pkt_size = sizeof(struct request_packet);

    for(int i=0;i<Max_User_Process_in_system;i++){

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
        req_pkt_set[i]->page_ref=0;
        req_pkt_set[i]->t=READ;
    }

    //initialize request Set

    // msg queues initialization
    sendQ_id = msgget(request_packet_msgQ_key,IPC_CREAT|0666);

    for(int i=0;i<Max_User_Process_in_system;i++)
        recvQ_id_set[i] = msgget((recv_msgQ_key_st+i),IPC_CREAT|0666);

    // initialize the sem for processes
    for(int i=0;i<Max_User_Process_in_system;i++){
        sem_process_id[i] = semget((sem_process_key_st+i),1,0666|IPC_CREAT);
        if(sem_process_id[i]==-1){
            perror("semget process error");
            clear_oss_shared_mem();
            exit(1);
        }

        int status = semctl(sem_process_id[i],0,SETVAL,0);
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
    total_avialable_frame=MAX_MEM;

    struct request_packet *p=NULL;

    while(keepRunning){

        // create the processes
        status = -2;
        if(process_in_systm==0 && process_itr==Max_User_Processes){
            keepRunning=FALSE;
            break;
        }

        if(process_in_systm <=Max_User_Process_in_system){
            status = setNewProcess();
            if(status == 1){
                process_in_systm++;
            }
            else if(status==-2 ){
                // print the error
//               printf("system full with process!!\n");
            }
            else if(status==-3 ){
                // print the error
            //    printf("Totoal processes created\n");
            }
            else if(status==-4){
//               printf("No Free Memory\n");
            }
        }
        handleQueue();

        p = read_packet(WAIT);

        if(p!=NULL){
            status = process_request(p);
        //     printf("Request Type:%d , CHILD:%d , status:%d\n",p->t,p->packet_id,status);
            write_pkt(status, p->packet_id);
        }
    }
    struct lclock st;
    st.sec=0;
    st.nsec=0;
    double tt = getTimeDiff(*lc_ptr,st);
    printf("Number Of Memory Access Pre Sec: %f\n",(stat_set.mem_access_count*1.0)/tt);
    printf("Numer of Page Faults per memory access: %f\n",(stat_set.page_faults*1.0)/stat_set.mem_access_count);
    printf("Number of Segmentation Faults per memory access: %f\n",(stat_set.seg_faults*1.0)/stat_set.mem_access_count);
    clear_oss_shared_mem();
    return 0;
}
