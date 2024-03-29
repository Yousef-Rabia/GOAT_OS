#include "headers.h"

void Termination_SIG(int signnum);
void newProc_arrive(int signum);
void algo_start();
void output_performance();
void write_in_logFile_start();
void write_in_logFile_resume();
void write_in_logFile_stop();

Process *curProc;
Queue *RQ;  //ready queue
Queue *WQ;  //queue for processes unable to initialize because of not having enough memory
int TempCLK;
PROC_BUFF newProc;
int msgBuf;
int algoId;
int mem_algo_id;
void (*algo_func)();
bool switched;
bool new_arrive = false;
bool cur_terminated = false;
int Quantum; // input from user when they choose round robin
// variables for the output files
FILE *log_file;
FILE *perf;
FILE *mem_log;
int process_count;
int tot_Wait = 0;
int tot_runtime = 0;
float *WTA_ARR; // array of WTA, used in scheduler.log math
int WTA_INDEX = 0;
LinkedList* memoryHoles;

void Buddy_Init();
//Memory Lists
LinkedList* All_Holes[9];
LinkedList *Holes_2;
LinkedList *Holes_1;
LinkedList *Holes_4;
LinkedList *Holes_8;
LinkedList *Holes_16;
LinkedList *Holes_32;
LinkedList *Holes_64;
LinkedList *Holes_128;
LinkedList *Holes_256;

void SRTN();
void HPF();
void RR();

int main(int argc, char *argv[])
{
    initClk();
    init_sim_state();
    
     
    signal(SIGUSR1, Termination_SIG);
    signal(SIGUSR2, newProc_arrive);

    RQ = Queue_init();
    WQ = Queue_init(); 
    algoId = atoi(argv[1]);
    mem_algo_id = atoi(argv[2]);
    // for round robin
    if (algoId == 3)
    {
        Quantum = atoi(argv[4]);
    }
    if(mem_algo_id == 1){
        
        memoryHoles = LinkedList_init();
        insertByStartAndEnd(memoryHoles,0,1023);
    }else{
       Buddy_Init();
    }
    msgBuf = init_buff();
    curProc == NULL;

    process_count = atoi(argv[3]);
    WTA_ARR = malloc(process_count * sizeof(float));

    perf = fopen("scheduler.log", "w");
    fprintf(perf, "#At time x process y state arr w total z remain y wait k\n");
    fclose(perf);

    mem_log = fopen("memory.log", "w");
    fprintf(mem_log, "#At time x allocated y bytes for process z from i to j\n");
    fclose(mem_log);

    printf("scheduler is working with algorithm id: %d and memory algorithm: %d\n", algoId, mem_algo_id);
    // TODO implement the scheduler :)
    // upon termination release the clock resources.
    TempCLK = getClk();
    algo_start();
}

void algo_start()
{
    // determine which algorithm to call
    switch (algoId)
    {
    case 1:
        algo_func = &SRTN;
        break;
    case 2:
        algo_func = &HPF;
        break;
    case 3:
        algo_func = &RR;
        break;
    default:
        break;
    }
    // looping every clk unit, if a new process arrives or a process terminates
    while (true)
    {
        if (getClk() != TempCLK || new_arrive || cur_terminated)
        {
            cur_terminated = false;
            TempCLK = getClk();
            // if there is no currently running process nor a ready one
            if (!(isEmpty(RQ) && curProc == NULL))
            {
                algo_func();
            }
            else if (isEmpty(RQ) && isEmpty(WQ) && *sim_state == 1) // ready queue is empty, and process generator is done sending
            {
                // outputting "scheduler.perf" and then ending the simulation
                output_performance();
                destroyClk(true);
            }
        }
    }
}

void SRTN()
{
    // if there is no currently process, pick the head of the ready queue
    if (curProc == NULL)
    {
        curProc = dequeue(RQ);
    }
    else if (!new_arrive)
    {
        // it doesn't update if a new process arrived cuz it is already updated
        curProc->remainingTime--;
    }
    new_arrive = false; // reset the flag

    if (!isEmpty(RQ) && curProc && curProc->remainingTime > front(RQ)->remainingTime)
    {
        // context switch

        switched = true;
        kill(curProc->pid, SIGSTOP);
        curProc->lastPreempt = getClk();
        curProc->p_state = Ready;

        write_in_logFile_stop();

        InsertWithPriority(RQ, curProc, curProc->remainingTime);
        curProc = dequeue(RQ);
        curProc->p_state = Running;
    }

    if (curProc && curProc->remainingTime == curProc->runningTime)
    {
        // create new process and run it
        switched = false;

        int pid = fork();
        if (pid == 0)
        {
            char t[8];
            sprintf(t, "%d", curProc->remainingTime);
            if (execlp("./process.out", "./process.out", t, NULL) == -1)
            {
                perror("error in runnign proc\n");
            }
        }
        curProc->pid = pid;
        curProc->waitingTime += getClk() - curProc->arrivalTime;
        curProc->p_state = Running;

        // output to the scheduler.log file
        write_in_logFile_start();
    }
    else
    {
        if (switched)
        {
            // resume the process if there is context switch
            switched = false;
            curProc->waitingTime += getClk() - curProc->lastPreempt;

            // output to the scheduler.log file
            write_in_logFile_resume();

            kill(curProc->pid, SIGCONT);
        }
    }
}

void HPF()
{
    if (new_arrive && curProc != NULL)
    {
        new_arrive = false;
        return;
    }
    // if there is no currently process, pick the head of the ready queue
    if (curProc == NULL)
    {
        curProc = dequeue(RQ);
    }
    else
    { // else you update the current process params
        curProc->remainingTime--;
    }

    if (curProc && curProc->remainingTime == curProc->runningTime)
    {
        // create new process and run it
        switched = false;
        int pid = fork();
        if (pid == 0)
        {
            char t[8];
            sprintf(t, "%d", curProc->remainingTime);
            if (execlp("./process.out", "./process.out", t, NULL) == -1)
            {
                perror("error in runnign proc\n");
            }
        }
        curProc->pid = pid;
        curProc->waitingTime += getClk() - curProc->arrivalTime;
        curProc->p_state = Running;

        // output to the scheduler.log file
        write_in_logFile_start();
    }
    else if (switched)
    {
        // resume the process if there is context switch
        switched = false;
        curProc->waitingTime += getClk() - curProc->lastPreempt;

        // output to the scheduler.log file
        write_in_logFile_resume();

        kill(curProc->pid, SIGCONT);
    }
}
void RR()
{
    // if there is no currently process, pick the head of the ready queue
    if (curProc == NULL)
    {
        curProc = dequeue(RQ);
    }
    else
    {
        if (!new_arrive) // it doesn't update if a new process arrived cuz it is already updated
            curProc->remainingTime--;
    }
    new_arrive = false; // reset the flag
    if (!isEmpty(RQ) && curProc && getClk() - curProc->lastResume == Quantum && curProc->remainingTime > 0)
    {
        // context switch
        switched = true;
        kill(curProc->pid, SIGSTOP);
        curProc->lastPreempt = getClk();
        curProc->p_state = Ready;

        // output to the scheduler.log file
        write_in_logFile_stop();

        enqueue(RQ, curProc);
        curProc = dequeue(RQ);
        curProc->p_state = Running;
    }
    if (curProc && curProc->remainingTime == curProc->runningTime)
    {
        // create new process and run it
        switched = false;
        int pid = fork();
        if (pid == 0)
        {
            char t[8];
            sprintf(t, "%d", curProc->remainingTime);
            if (execlp("./process.out", "./process.out", t, NULL) == -1)
            {
                perror("error in runnign proc\n");
            }
        }
        curProc->pid = pid;
        curProc->waitingTime += getClk() - curProc->arrivalTime;
        curProc->p_state = Running;
        curProc->lastResume = getClk();

        // output to the scheduler.log file
        write_in_logFile_start();
    }
    else
    {
        if (switched)
        {
            // resume the process if there is context switch
            switched = false;
            curProc->waitingTime += getClk() - curProc->lastPreempt;
            curProc->lastResume = getClk();

            // output to the scheduler.log file
            write_in_logFile_resume();

            kill(curProc->pid, SIGCONT);
        }
    }
}

bool allocateMemory(Process *p)
{
    LL_Node *curr = memoryHoles->head;
    while (curr != NULL) {

        Hole *hole = (Hole *)(curr->data);
        int block_size = hole->end - hole->start + 1;    //need review
        if (block_size >= p->memsize) {
            // allocate memory block
            p->memBlock.start =  hole->start;
            p->memBlock.end = hole->start + p->memsize - 1;
            p->memBlock.size = p->memsize;

            hole->start += p->memsize;
            if (hole->start > hole->end) {
                // remove node if it is fully allocated
                removeNode(memoryHoles, curr);
            }
            return true;
        }
        curr = curr->next;
    }
    // no suitable memory block found
    return false;
}

//deallocate
void freeMemory(Process *p)
{
    // insert new node into memoryHoles linked list
    LL_Node* new_node = insertByStartAndEnd(memoryHoles,p->memBlock.start,p->memBlock.end);
    Hole *curr_hole = (Hole*)(new_node->data);
    
    // merge with next node if it is free 
    if (new_node->next != NULL) {
        Hole *next_hole = (Hole*)(new_node->next->data);
        if (next_hole->start == curr_hole->end + 1) {
            curr_hole->end = next_hole->end;
            removeNode(memoryHoles, new_node->next);
        }
    }

    // merge with previous node if it is free
    if (new_node->prev != NULL) {
        Hole *prev_hole = (Hole*)(new_node->prev->data);
        if (prev_hole->end == curr_hole->start - 1) {
            prev_hole->end = curr_hole->end;
            removeNode(memoryHoles, new_node);
        }
    }
}

bool First_Fit(Process *p)
{
    // Allocate memory using first fit algorithm
    bool result = allocateMemory(p);
    if (result) {
        //printing in the memory log file
        mem_log = fopen("memory.log", "a");
        fprintf(mem_log, "At time %d allocated %d bytes for process %d from %d to %d\n",
            getClk(),
            p->memBlock.size,
            p->id,
            p->memBlock.start,
            p->memBlock.end
    );
    fclose(mem_log);
    } else {
        printf("Unable to allocate memory for process %d\n", p->id);
    }
    return result;
}

void Buddy_Init(){
    Holes_1=LinkedList_init();
    All_Holes[0]=Holes_1;
    Holes_2=LinkedList_init();
    All_Holes[1]=Holes_2;
    Holes_4=LinkedList_init();
    All_Holes[2]=Holes_4;
    Holes_8=LinkedList_init();
    All_Holes[3]=Holes_8;
    Holes_16=LinkedList_init();
    All_Holes[4]=Holes_16;
    Holes_32=LinkedList_init();
    All_Holes[5]=Holes_32;
    Holes_64=LinkedList_init();
    All_Holes[6]=Holes_64;
    Holes_128=LinkedList_init();
    All_Holes[7]=Holes_128;
    Holes_256=LinkedList_init();


    LL_Node *hole256_1=newLLNode(768,1023);
    hole256_1->next=NULL;
    LL_Node *hole256_2=newLLNode(512,767);
    hole256_2->next=hole256_1;
    LL_Node *hole256_3=newLLNode(256,511);
    hole256_3->next=hole256_2;
    LL_Node *hole256_4=newLLNode(0,255);
    hole256_4->next=hole256_3;

    Holes_256->size=4;
    Holes_256->head=hole256_4;
    All_Holes[8]=Holes_256;
}
void Partition(int cur_hole_idx,int needed_hole_idx){
    while (cur_hole_idx != needed_hole_idx){
        LL_Node* toSplit =All_Holes[cur_hole_idx]->head;
        All_Holes[cur_hole_idx]->head=All_Holes[cur_hole_idx]->head->next;
        All_Holes[cur_hole_idx]->size-=1;
        int newSize=toSplit->data->size/2;
        LL_Node* new_2=newLLNode(toSplit->data->start+ newSize,toSplit->data->end);
        new_2->next=All_Holes[cur_hole_idx-1]->head;
        LL_Node* new_1=newLLNode(toSplit->data->start,new_2->data->start-1);
        new_1->next=new_2;
        All_Holes[cur_hole_idx-1]->head=new_1;
        All_Holes[cur_hole_idx-1]->size+=2;
        cur_hole_idx--;
    }
}
bool Buddy(Process* proc)
{
    int Original_Size=proc->memsize;
    int Needed_Hole_idx=ceil(log2(Original_Size));
    int cur_hole_idx=Needed_Hole_idx;
    if(cur_hole_idx >= 9)
    {
        printf("Process requires more than 256 bytes!\n");
        return false;
    }

    while (cur_hole_idx<9 && isLLEmpty(All_Holes[cur_hole_idx])){
        cur_hole_idx++;
    }
    if(cur_hole_idx == 9)
        return false;
    if(cur_hole_idx!=Needed_Hole_idx)
        Partition(cur_hole_idx,Needed_Hole_idx);

    MemBlock* Allocated_Block=newMemBlock(All_Holes[Needed_Hole_idx]->head);
    LL_Node* temp =All_Holes[Needed_Hole_idx]->head;
    All_Holes[Needed_Hole_idx]->head=All_Holes[Needed_Hole_idx]->head->next;
    All_Holes[Needed_Hole_idx]->size-=1;
    proc->memBlock=*Allocated_Block;
    
    //printing in the memory log file
    mem_log = fopen("memory.log", "a");
    fprintf(mem_log, "At time %d allocated %d bytes for process %d from %d to %d\n",
            getClk(),
            Allocated_Block->size,
            proc->id,
            Allocated_Block->start,
            Allocated_Block->end
    );
    fclose(mem_log);

    free(temp);
    return true;
}

void Rmv(LinkedList* ll ,LL_Node* node){
    LL_Node* ptr=ll->head;
    LL_Node* prev=ll->head;
    if(ptr==node){
        ll->head=ptr->next;
        free(ptr);
        ll->size-=1;
        return;
    }
    while(ptr){
        if(ptr==node){
            LL_Node* temp=ptr;
            prev->next=ptr->next;
            free(temp);
            ll->size-=1;
            return;
        }
        prev=ptr;
        ptr=ptr->next;
    }
}
void Buddy_dealloction(LL_Node *proc)
{
    LL_Node *DeNode = proc;
    int st = DeNode->data->start;
    int end = DeNode->data->end;
    int neededIdx = log2(proc->data->size);
    LinkedList *ll = All_Holes[neededIdx];
   
   
    if (!ll->head)
    {
        ll->head = DeNode;
        ll->size += 1;
        return;
    }
    LL_Node *prev = ll->head;
    LL_Node *ptr = ll->head->next;
    if (ll->head->data->start > DeNode->data->end)
    {
        DeNode->next = ll->head;
        ll->head = DeNode;
        ll->size += 1;
    }
    else
    {
        while (ptr)
        {
            if (ptr->data->start > end)
            {
                prev->next = DeNode;
                DeNode->next = ptr;
                ll->size += 1;
                break;
            }
            ptr = ptr->next;
            prev = prev->next;
        }
        if (!ptr)
        {
            prev->next = DeNode;
            ll->size += 1;
        }
    }
    if(neededIdx==8)
    {    
        return;
    }  
  bool IsLeft = !((proc->data->start / proc->data->size) % 2);
    if (IsLeft)
    {
        if (DeNode->next)
        {
            if (DeNode->next->data->start == end + 1)
            {
                LL_Node *NewNode = newLLNode(st, DeNode->next->data->end);
                Rmv(ll,DeNode);
                Rmv(ll,DeNode->next);
                Buddy_dealloction(NewNode);
            }
        }
    }
    else
    {
        if (st == prev->data->end + 1)
        {
            LL_Node *NewNode = newLLNode(prev->data->start, end);
            Rmv(ll,DeNode);
            Rmv(ll,prev);
            Buddy_dealloction(NewNode);
        }
    }
}

void Termination_SIG(int signnum)
{
    int TA = getClk() - curProc->arrivalTime;
    float WeTA = (float)TA / curProc->runningTime;

    tot_Wait += curProc->waitingTime;
    tot_runtime += curProc->runningTime;
    WTA_ARR[WTA_INDEX] = WeTA;
    WTA_INDEX++;

    // output to the "scheduler.log" file
    log_file = fopen("scheduler.log", "a");
    fprintf(log_file, "At time %d process %d finished arr %d total %d remain %d wait %d TA %d WTA %f\n",
            getClk(),
            curProc->id,
            curProc->arrivalTime,
            curProc->runningTime,
            curProc->remainingTime,
            curProc->waitingTime,
            TA,
            WeTA);
    fclose(log_file);

        //deallocate the memory
        LL_Node *removed = newLLNode(curProc->memBlock.start, curProc->memBlock.end);
        if(mem_algo_id == 1){
            freeMemory(curProc);
        }else{
            Buddy_dealloction(removed);
        }
        //printing in the memory log file
    mem_log = fopen("memory.log", "a");
    fprintf(mem_log, "At time %d freed %d bytes from process %d from %d to %d\n",
            getClk(),
            curProc->memBlock.size,
            curProc->id,
            curProc->memBlock.start,
            curProc->memBlock.end
    );
    fclose(mem_log);

    free(curProc);
    curProc = NULL;
    switched = true;
    cur_terminated = true;

    while(!isEmpty(WQ))
    {
    if(mem_algo_id == 1)
    {
        if(First_Fit(front(WQ)))
        {
            Process* p = dequeue(WQ);
            if (algoId == 1)
            {
                InsertWithPriority(RQ, p, p->remainingTime);
            }
            else if (algoId == 2)
            {
                InsertWithPriority(RQ, p, p->priority);
            }
            else if (algoId == 3)
            {
                enqueue(RQ, p);
            }
        }
        else
            break;
    }
    else
    {
        if(Buddy(front(WQ)))
        {
            Process* p = dequeue(WQ);
            if (algoId == 1)
            {
                InsertWithPriority(RQ, p, p->remainingTime);
            }
            else if (algoId == 2)
            {
                InsertWithPriority(RQ, p, p->priority);
            }
            else if (algoId == 3)
            {
                enqueue(RQ, p);
            }
        }
        else 
            break;
    }
    }
}

void newProc_arrive(int signnum)
{
    struct msqid_ds stat;
    int i;
    do
    {
        int rcv = msgrcv(msgBuf, &newProc, sizeof(newProc.proc), 1, !IPC_NOWAIT);
        if (rcv == -1)
        {
            perror("Error in receive\n");
            exit(-1);
        }
        msgctl(msgBuf, IPC_STAT, &stat);
        i = stat.msg_qnum;
        Process *p = (Process *)malloc(sizeof(Process));
        p->arrivalTime = newProc.proc.arrivalTime;
        p->id = newProc.proc.id;
        p->priority = newProc.proc.priority;
        p->remainingTime = newProc.proc.remainingTime;
        p->runningTime = newProc.proc.runningTime;
        p->waitingTime = newProc.proc.waitingTime;
        p->memsize = newProc.proc.memsize;

        bool allocation_result = false;
        if (mem_algo_id == 1)
        {
            allocation_result = First_Fit(p);
        }
        else
        {
            allocation_result = Buddy(p);
        }
        // to be modified based on algoId
        if (allocation_result)
        {
            if (algoId == 1)
            {
                InsertWithPriority(RQ, p, p->remainingTime);
            }
            else if (algoId == 2)
            {
                InsertWithPriority(RQ, p, p->priority);
            }
            else if (algoId == 3)
            {
                enqueue(RQ, p);
            }
        }
        else
        {
            enqueue(WQ, p);
        }
    } while (i);
    new_arrive = true;
}

// outputs the scheduler.perf at the end of the simulation
void output_performance()
{
    int curr_time = getClk();
    float CPU_utilization = (tot_runtime / (float)curr_time) * 100;
    float avg_wait = (float)tot_Wait / process_count;
    float WTA_SUM = 0;
    float sum_squared_error = 0;
    for (int i = 0; i < process_count; i++)
    {
        WTA_SUM += WTA_ARR[i];
    }
    float WTA_AVG = WTA_SUM / process_count;
    for (int i = 0; i < process_count; i++)
    {
        sum_squared_error += pow((WTA_ARR[i] - WTA_AVG), 2);
    }
    float WTA_STD = sqrt(sum_squared_error / process_count);

    printf("proc_count: %d curr_time: %d tot_runtime: %d tot_wait: %d\n", process_count, curr_time, tot_runtime, tot_Wait);
    perf = fopen("scheduler.perf", "w");
    fprintf(perf, "CPU utilization = %f\n", CPU_utilization);
    fprintf(perf, "Avg WTA = %f\n", WTA_AVG);
    fprintf(perf, "Avg Waiting = %f\n", avg_wait);
    fprintf(perf, "Std WTA = %f\n", WTA_STD);
    fclose(perf);
}

void write_in_logFile_start()
{
    // output to the scheduler.log file
    log_file = fopen("scheduler.log", "a");
    fprintf(log_file, "At time %d process %d started arr %d total %d remain %d wait %d\n",
            getClk(),
            curProc->id,
            curProc->arrivalTime,
            curProc->runningTime,
            curProc->remainingTime,
            curProc->waitingTime);
    fclose(log_file);
}
void write_in_logFile_stop()
{
    // output to the scheduler.log file
    log_file = fopen("scheduler.log", "a");
    fprintf(log_file, "At time %d process %d stopped arr %d total %d remain %d wait %d\n",
            getClk(),
            curProc->id,
            curProc->arrivalTime,
            curProc->runningTime,
            curProc->remainingTime,
            curProc->waitingTime);
    fclose(log_file);
}
void write_in_logFile_resume()
{
    log_file = fopen("scheduler.log", "a");
    fprintf(log_file, "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
            getClk(),
            curProc->id,
            curProc->arrivalTime,
            curProc->runningTime,
            curProc->remainingTime,
            curProc->waitingTime);
    fclose(log_file);
}