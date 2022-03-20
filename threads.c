#include "ec440threads.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <setjmp.h>
#include <stdio.h>
#include <sys/time.h>
#include <errno.h>

/* You can support more threads. At least support this many. */
#define MAX_THREADS 128

/* Your stack should be this many bytes in size */
#define THREAD_STACK_SIZE 32767

/* Number of microseconds between scheduling events */
#define SCHEDULER_INTERVAL_USECS (50 * 1000)

/* Extracted from private libc headers. These are not part of the public
 * interface for jmp_buf.*/
#define JB_RBX 0
#define JB_RBP 1
#define JB_R12 2
#define JB_R13 3
#define JB_R14 4
#define JB_R15 5
#define JB_RSP 6
#define JB_PC 7

enum thread_status
{
	TS_EXITED,
	TS_RUNNING,
	TS_READY,
    TS_BLOCKED
};

struct thread_control_block {
	/* TODO: add a thread ID */
    pthread_t tid;
	/* TODO: add information about its stack */
    void* stackfree;
	/* TODO: add information about its registers */
    jmp_buf buf;
	/* TODO: add information about the status (e.g., use enum thread_status) */
	enum thread_status stat;
    /* Add other information you need to manage this thread */
};

struct bar {
    
    int id;
    
    unsigned int numwait;
    
    pthread_t waiting[MAX_THREADS];
    
    unsigned int cap;
    
};

struct mut {
    
    int numwait;
    
    pthread_t waiting[MAX_THREADS];
    
};

//Global variables (sue me)
struct itimerval* salarm;
struct sigaction* catch;
struct thread_control_block *threads[MAX_THREADS + 1];
struct bar *barriers[MAX_THREADS];
static int current_thread_id = 0;
static int signore = 0; //Used to prevent scheduler interrupting thread creation/destruction/inspection, as bad stuff could result.
static int threadcount = 1;
static int barrier_count = 0;

// to supress compiler error
static void schedule(int signal) __attribute__((unused));

static void schedule(int signal)
{
    //printf("Scheduling . . . \n");
    if(signore){
        return;
    }
    int threadpick = -1;
    int threaddex = current_thread_id + 1;
    int startdex = current_thread_id;
    while(threadpick < 0){ //Round robin scheduler
        if(threaddex > MAX_THREADS){
            threaddex = 0;
            continue;
        }
        if(threads[threaddex] == NULL){
            threaddex++;
            continue;
        }
        if(threaddex == startdex){
            if(threads[startdex]->stat == TS_EXITED){exit(0);}
            else if(threads[startdex]->stat == TS_READY || threads[startdex]->stat == TS_RUNNING){threadpick = startdex;}
            else{threadpick = 0;}
            break;
        }
        if((threads[threaddex]->stat == TS_READY)){
            threadpick = threaddex;
        }
        threaddex++;
    }
    if(!(threads[current_thread_id]->stat == TS_EXITED)){
        //IF THE THREAD HAS NOT EXITED< WE MUST USE SETJMP TO UPDATE THE BUFFER
        //OTHERWISE, WE DONT WANT TO UPDATE AN EXITED THREAD, THAT LEADS TO UNWANTED BEHAVIOR
        struct thread_control_block* cthread = threads[current_thread_id];
        if(cthread->stat != TS_BLOCKED){
            cthread->stat = TS_READY;
        }
        if(!setjmp(cthread->buf)){
            
        }
        else{
            //Return from longjmp
            return;
        }
    }
    //Now jump to new thread
    struct thread_control_block* mthread = threads[threadpick];
    mthread->stat = TS_RUNNING;
    current_thread_id = threadpick;
    longjmp(mthread->buf, 1);
    
    return;
}

static void scheduler_init()
{
    int i;
    for(i = 1; i < MAX_THREADS + 1; i++){
        threads[i] = NULL;
    }
    salarm = malloc(sizeof(struct itimerval));
    salarm->it_value.tv_sec = 0;
    salarm->it_value.tv_usec = SCHEDULER_INTERVAL_USECS;
    salarm->it_interval.tv_sec = 0;
    salarm->it_interval.tv_usec = SCHEDULER_INTERVAL_USECS;
    if(setitimer(ITIMER_REAL, salarm, NULL)){perror("BAD ALARM:");}
    catch = calloc(1, sizeof(struct sigaction));
    catch->sa_flags = SA_NODEFER;
    catch->sa_handler = schedule;
    if(sigaction(SIGALRM, catch, NULL)){perror("BAD ACTION:");}
}

int pthread_create(
	pthread_t *thread, const pthread_attr_t *attr,
	void *(*start_routine) (void *), void *arg)
{
	// Create the timer and handler for the scheduler. Create thread 0.
	static bool is_first_call = true;
    signore = 1;
	if (is_first_call)
	{
		scheduler_init();
        threads[0] = malloc(sizeof(struct thread_control_block));
	}
    if(threadcount > MAX_THREADS){
        return -1;
    }
    threads[threadcount] = malloc(sizeof(struct thread_control_block));
    struct thread_control_block* cthread = threads[threadcount];
    cthread->tid = threadcount;
    cthread->stat = TS_READY;
    cthread->buf->__jmpbuf[JB_PC] = ptr_mangle((unsigned long int) start_thunk);
    cthread->buf->__jmpbuf[JB_R12] = (unsigned long int) *start_routine;
    cthread->buf->__jmpbuf[JB_R13] = (unsigned long int) arg;
    cthread->stackfree = malloc(THREAD_STACK_SIZE);
    cthread->buf->__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int) (cthread->stackfree + THREAD_STACK_SIZE - sizeof(pthread_exit)));
    *((long unsigned int*) ptr_demangle(cthread->buf->__jmpbuf[JB_RSP])) = (long unsigned int) pthread_exit;
    *thread = threadcount;
    threadcount++;
    if (is_first_call)
	{
		is_first_call = false;
        struct thread_control_block* mthread = threads[0];
        mthread->tid = 0;
        mthread->stat = TS_RUNNING;
        mthread->stackfree = NULL;
        if(!setjmp(mthread->buf)){
        //First run through
        }
        else{
        //Returning from longjmp
        //No action needs to be taken on return
        
        }
    }
    signore = 0;
	return 0;
}

void pthread_exit(void *value_ptr)
{
    signore = 1;
    if(threads[current_thread_id]->stackfree != NULL){
        free(threads[current_thread_id]->stackfree);
    }
    threads[current_thread_id]->stat = TS_EXITED;
    signore = 0;
    schedule(SIGALRM);
    free(catch);
    free(salarm);
    for(int k = 0; k < MAX_THREADS + 1; k++){
        free(threads[k]);
    }
	exit(0);
}

pthread_t pthread_self(void)
{
    signore = 1;
    int thread_at_time = (threads[current_thread_id])->tid;
    signore = 0;
	return thread_at_time;
}

int pthread_mutex_init(pthread_mutex_t *restrict mutex, const pthread_mutexattr_t *restrict attr)
{
    signore = 1;
    mutex = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    
    mutex->__data.__lock = 0;
    mutex->__data.__list.__next = malloc(sizeof(struct mut));
    ((struct mut*) mutex->__data.__list.__next)->numwait = 0;
    
    signore = 0;
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    signore = 1;
    if(mutex->__data.__lock == 1){
        struct mut* tpo = (struct mut*) mutex->__data.__list.__next;
        (tpo->waiting)[tpo->numwait] = pthread_self();
        tpo->numwait++;
        threads[pthread_self()]->stat = TS_BLOCKED;
        signore = 0;
        schedule(SIGALRM);
    }
    else{
        mutex->__data.__lock = 1;
    }
    signore = 0;
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    signore = 1;
    if(((struct mut*) mutex->__data.__list.__next)->numwait != 0){
        for(int i = 0; i < ((struct mut*) mutex->__data.__list.__next)->numwait; i++){
            threads[((struct mut*) mutex->__data.__list.__next)->waiting[i]]->stat = TS_READY;
        }
        ((struct mut*) mutex->__data.__list.__next)->numwait = 0;
    }
    mutex->__data.__lock = 0;
    signore = 0;
    return 0;
}
    
int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
    signore = 1;
    //free(((struct mut*) mutex->__align));
    //free(mutex);
    signore = 0;
    return 0;
}

int pthread_barrier_init(pthread_barrier_t *restrict barrier, const pthread_barrierattr_t *restrict attr, unsigned int count)
{    
    if(count == 0){return EINVAL;}
    signore = 1;
    barrier = (pthread_barrier_t *) malloc(sizeof(pthread_barrier_t));
    barriers[barrier_count] = (struct bar *) malloc(sizeof(struct bar));
    barrier->__align = barrier_count;
    barriers[barrier_count]->id = barrier_count;
    barriers[barrier_count]->cap = count;
    barriers[barrier_count]->numwait = 0;
    barrier_count++;
    signore = 0;
    return 0;
}

int pthread_barrier_wait(pthread_barrier_t *barrier)
{
    int is_final_caller = 0;
    signore = 1;
    if(barriers[barrier->__align]->numwait + 1 == barriers[barrier->__align]->cap){
        is_final_caller = 1;
        for(int i = 0; i < barriers[barrier->__align]->numwait; i++){
            threads[barriers[barrier->__align]->waiting[i]]->stat = TS_READY;
        }
        barriers[barrier->__align]->numwait = 0;
    }
    else{
        barriers[barrier->__align]->waiting[barriers[barrier->__align]->numwait] = pthread_self();
        threads[pthread_self()]->stat = TS_BLOCKED;
        barriers[barrier->__align]->numwait++;
        signore = 0;
        schedule(SIGALRM);
    }
    signore = 0;
    if(is_final_caller){return PTHREAD_BARRIER_SERIAL_THREAD;}
    return 0;
}

int pthread_barrier_destroy(pthread_barrier_t *barrier)
{
    signore = 1;
    //free(barriers[barrier->__align]);
    //free(barrier);
    signore = 0;
    return 0;
}