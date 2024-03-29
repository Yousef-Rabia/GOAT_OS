#include "headers.h"

void clearResources(int);
int clk_pid, sch_pid;
int msgBuf;
PROC_BUFF newProc;
int sim_state_id;

int main(int argc, char *argv[])
{
    Queue process_queue = *Queue_init();
    signal(SIGINT, clearResources);
    sim_state_id = init_sim_state();
    int initial_state = 0;
    *sim_state = initial_state;
    msgBuf = init_buff();
    newProc.m_type = 1;
    int process_count = 0; // the number of processes in the simulation, sent to the scheduler to create WTA_ARR accordingly

    // TODO Initialization
    // 1. Read the input files.
    FILE *ptr = fopen(argv[1], "r");
    if (ptr == NULL)
    {
        printf("no such file.");
        return 0;
    }

    char p_id[16], p_arr[16], p_run[16], p_pri[16], p_mem[16];
    while (fscanf(ptr, "%s %s %s %s %s", p_id, p_arr, p_run, p_pri, p_mem) == 5)
    {
        // ignore comments
        if (p_id[0] == '#')
            continue;
        Process *p = (Process *)malloc(sizeof(Process));
        p->arrivalTime = atoi(p_arr);
        p->id = atoi(p_id);
        p->priority = atoi(p_pri);
        p->runningTime = atoi(p_run);
        p->memsize = atoi(p_mem);
        p->remainingTime = p->runningTime;
        p->p_state = Ready;
        process_count++;
        enqueue(&process_queue, p);
    }

    // 2. Use the command line arguments to get the chosen scheduling algorithm and its parameters, if there are any.
    // format is as mentioned in the faq.
    char algo_id[2], mem_algo_id[2], quantum[2];
    algo_id[0] = argv[3][0];
    if(algo_id[0] == '3')
    {
        quantum[0] = argv[5][0];
        mem_algo_id[0] = argv[7][0];
    }
    else
    {
        mem_algo_id[0] = argv[5][0];
    }

    // 3. Initiate and create the scheduler and clock processes.
    sch_pid = fork();
    if (sch_pid == -1)
    {
        printf("error in creating scheduler");
    }
    else if (sch_pid == 0)
    {
        char proc_count[8];
        sprintf(proc_count, "%d", process_count);
        if (algo_id[0] == '3')
        {
            execlp("./scheduler.out", "./scheduler.out", algo_id, mem_algo_id, proc_count, quantum, NULL);
        }
        else
            execlp("./scheduler.out", "./scheduler.out", algo_id, mem_algo_id, proc_count, NULL);
    }

    clk_pid = fork();
    if (sch_pid == -1)
    {
        printf("error in creating scheduler");
    }
    else if (clk_pid == 0)
        execlp("./clk.out", "./clk.out", NULL);

    // 4. Use this function after creating the clock process to initialize clock
    initClk();
    // To get time use this
    int x = getClk();
    // TODO Generation Main Loop
    // 5. Create a data structure for processes and provide it with its parameters.
    // DONE AT THE BEGINNING

    // 6. Send the information to the scheduler at the appropriate time.
    int current_time = getClk();
    while (!isEmpty(&process_queue))
    {
        // if the current time = the arrival of a process send it to the queue.
        if (current_time != getClk())
        {
            current_time = getClk();
        }
        while (!isEmpty(&process_queue) && getClk() == front(&process_queue)->arrivalTime)
        {
            // dequeue and send the top process to the scheduler's ready queue
            Process *toSnd = dequeue(&process_queue);
            newProc.proc.id = toSnd->id;
            newProc.proc.arrivalTime = toSnd->arrivalTime;
            newProc.proc.p_state = toSnd->p_state;
            newProc.proc.priority = toSnd->priority;
            newProc.proc.remainingTime = toSnd->remainingTime;
            newProc.proc.runningTime = toSnd->runningTime;
            newProc.proc.memsize = toSnd->memsize;
            newProc.proc.lastPreempt = 0;
            newProc.proc.waitingTime = 0;

            int snd = msgsnd(msgBuf, &newProc, sizeof(newProc.proc), !IPC_NOWAIT);
            if (snd == -1)
            {
                perror("Error in Send!\n");
                exit(-1);
            }
            // send the signal if this is the last process the generator will be sending this second
            if (!(!isEmpty(&process_queue) && getClk() == front(&process_queue)->arrivalTime))
                kill(sch_pid, SIGUSR2);
        }
    }
    // 7. Clear clock resources
    // switch sim state to let the scheduler know that no more processes will be sent and wait for it to die before clearing IPCs
    *sim_state = 1;
    int stat;
    waitpid(sch_pid, &stat, 0);

    // clear all resources
    kill(getpid(), SIGINT);
}

void clearResources(int signum)
{
    // TODO Clears all resources in case of interruption
    msgctl(msgBuf, IPC_RMID, (struct msqid_ds *)0);
    shmctl(sim_state_id, IPC_RMID, (struct shmid_ds *)0);
    kill(clk_pid, SIGINT);
    exit(0);
}
