#include "headers.h"

void Termination_SIG(int signnum);
void newProc_arrive(int signum);

Process *curProc;
Queue *RQ;
int TempCLK;
bool IS_preempted;
PROC_BUFF newProc;
int msgBuf;
int algoId;
bool switched;


void SRTN();

int main(int argc, char *argv[])
{
    initClk();

    signal(SIGUSR1, Termination_SIG);
    signal(SIGUSR2, newProc_arrive);

    RQ = Queue_init();
    algoId = atoi(argv[1]);

    msgBuf = init_buff();
    curProc == NULL;

    printf("scheduler is working with algorithm id: %s\n", argv[1]);
    // TODO implement the scheduler :)
    // upon termination release the clock resources.
    // remove this later
    TempCLK = getClk();
    while (true)
    {
        if (getClk() != TempCLK)
        {
            TempCLK = getClk();
            if (!(isEmpty(RQ) && curProc == NULL))
            {
                SRTN();
            }
        }
    }

    destroyClk(true);
}

void SRTN(){
    if (curProc == NULL)
    {
        curProc = dequeue(RQ);

    }
    else
    {
        curProc->remainingTime--;
    }
    
    if (!isEmpty(RQ) && curProc && curProc->remainingTime > front(RQ)->remainingTime)
    {
        // context switch
        
        switched = true;
        kill(curProc->pid,SIGSTOP);
        curProc->lastPreempt = getClk();
        curProc->p_state = Ready;

        printf("At time %d process %d stopped arr %d total %d remain %d wait %d\n",
               getClk(),
               curProc->id,
               curProc->arrivalTime,
               curProc->runningTime,
               curProc->remainingTime,
               curProc->waitingTime);

        InsertWithPriority(RQ, curProc, curProc->remainingTime);
        curProc = dequeue(RQ);
        curProc->p_state = Running;
    }
    if (curProc &&curProc->remainingTime == curProc->runningTime)
    {
        // create new process and run it
        switched=false;
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
          printf("At time %d process %d started arr %d total %d remain %d wait %d\n",
                   getClk(),
                   curProc->id,
                   curProc->arrivalTime,
                   curProc->runningTime,
                   curProc->remainingTime,
                   curProc->waitingTime);
    }
    else
    {
        if (switched )
        {
            switched=false;
            curProc->waitingTime += getClk() - curProc->lastPreempt;
            printf("At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                   getClk(),
                   curProc->id,
                   curProc->arrivalTime,
                   curProc->runningTime,
                   curProc->remainingTime,
                   curProc->waitingTime);
            kill(curProc->pid, SIGCONT);
        }
    }
    // printf("\nTest\n");
}

void Termination_SIG(int signnum)
{
    int TA = getClk() - curProc->arrivalTime;
    float WeTA = (float)TA / curProc->runningTime;

    WTA += WeTA;
    WaitingTime += curProc->waitingTime;
    // print on screen
    printf("At time %d process %d finished arr %d total %d remain %d wait %d TA %d WTA %f\n",
           getClk(),
           curProc->id,
           curProc->arrivalTime,
           curProc->runningTime,
           curProc->remainingTime,
           curProc->waitingTime,
           TA,
           WeTA);
    free(curProc);
    curProc=NULL;
    switched=true;
    //call the algo one more time
    SRTN();
}

void newProc_arrive(int signnum)
{
    int rcv = msgrcv(msgBuf, &newProc, sizeof(newProc.proc), 1, !IPC_NOWAIT);
    if (rcv == -1)
    {
        perror("Error in reveive\n");
        exit(-1);
    }
    Process *p = (Process *)malloc(sizeof(Process));
    p->arrivalTime = newProc.proc.arrivalTime;
    p->id = newProc.proc.id;
    p->priority = newProc.proc.priority;
    p->remainingTime = newProc.proc.remainingTime;
    p->runningTime = newProc.proc.runningTime;
    p->waitingTime = newProc.proc.waitingTime;

    // to be modified based on algo_id
    InsertWithPriority(RQ, p,p->remainingTime);
    SRTN();
}