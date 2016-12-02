/** @file libscheduler.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libscheduler.h"
#include "../libpriqueue/libpriqueue.h"


/**
  Stores information making up a job to be scheduled including any statistics.

  You may need to define some global variables or a struct to store your job queue elements. 
*/
typedef struct _job_t
{
  int id;
  int firstResponse;
  int actualResponseTime;
  double initialTime;
  double startTime;
  double runningTime;
  int priority;

} job_t;

typedef struct _core_t{
  int isIdle;
  job_t *myJob;
} core_t;

int FCFSComparer(const void * a,const void * b){
   job_t *one=(job_t *)a;
   job_t *two=(job_t*)b;
   return (one->startTime)-(two->startTime);
}
int SJFComparer(const void * a,const void * b){
   job_t *one=(job_t *)a;
   job_t *two=(job_t*)b;
   if(one->runningTime==two->runningTime){
      return (one->initialTime)-(two->initialTime);
   }
   return (one->runningTime)-(two->runningTime);
}
int PSJFComparer(const void * a,const void * b){
   job_t *one=(job_t *)a;
   job_t *two=(job_t*)b;
   if(one->runningTime==two->runningTime){
      return (one->initialTime)-(two->initialTime);
   }
   return (one->runningTime)-(two->runningTime);
}
int PRIComparer(const void * a,const void * b){
   job_t *one=(job_t *)a;
   job_t *two=(job_t*)b;
   if(one->priority==two->priority){
      return (one->initialTime)-(two->initialTime);
   }
   return (one->priority)-(two->priority);
}
int PPRIComparer(const void * a,const void * b){
   job_t *one=(job_t *)a;
   job_t *two=(job_t*)b;
   if(one->priority==two->priority){
      return (one->initialTime)-(two->initialTime);
   }
   return (one->priority)-(two->priority);
}
int RRComparer(const void * a,const void * b){
   return 1;
}

// TODO This is a good place for some global variables? Like number of jobs,
// number of cores, total waiting time, total response time, etc.

int numJobs;
int numCores;
double totalWaitTime=0;//hello
double totalResponseTime=0;
double totalTurnaroundTime=0;
priqueue_t *priorityQueue;
core_t *cores;
scheme_t myScheme;
/**
  Initalizes the scheduler.
 
  Assumptions:
    - You may assume this will be the first scheduler function called.
    - You may assume this function will be called once once.
    - You may assume that cores is a positive, non-zero number.
    - You may assume that scheme is a valid scheduling scheme.

  @param cores the number of cores that is available by the scheduler. These
    cores will be known as core(id=0), core(id=1), ..., core(id=cores-1).
  @param scheme  the scheduling scheme that should be used. This value will be
    one of the six enum values of scheme_t
*/
void scheduler_start_up(int nCores, scheme_t scheme)
{
  priorityQueue=malloc(sizeof(priqueue_t));
  numJobs=0;
  if(scheme==FCFS){
    priqueue_init(priorityQueue,FCFSComparer);
  }else if(scheme==SJF){
    priqueue_init(priorityQueue,SJFComparer);
  }else if(scheme==PSJF){//preemptive shortest job
    priqueue_init(priorityQueue,PSJFComparer);
  }else if(scheme==PRI){
   priqueue_init(priorityQueue,PRIComparer);
  }else if(scheme==PPRI){//preemptive highest priority
    priqueue_init(priorityQueue,PPRIComparer);
  }else{
    priqueue_init(priorityQueue,RRComparer);
  }
  myScheme=scheme;
  numCores=nCores;
  cores=(core_t *)malloc(sizeof(core_t)*numCores);
  int i;
  for(i=0;i<numCores;i++){
    cores[i].myJob=NULL;
    cores[i].isIdle=0;
   }
}


/**
  Called when a new job arrives.
 
  If multiple cores are idle, the job should be assigned to the core with the
  lowest id.
  If the job arriving should be scheduled to run during the next
  time cycle, return the zero-based index of the core the job should be
  scheduled on. If another job is already running on the core specified,
  this will preempt the currently running job.
  Assumptions:
    - You may assume that every job wil have a unique arrival time.

  @param job_number a globally unique identification number of the job arriving.
  @param time the current time of the simulator.
  @param running_time the total number of time units this job will run before it
    will be finished.
  @param priority the priority of the job. (The lower the value, the higher the
    priority.)
  @return index of core job should be scheduled on
  @return -1 if no scheduling changes should be made. 
 
 */
int scheduler_new_job(int job_number, int time, int running_time, int priority)
{
   numJobs++;
   job_t *job=(job_t *)malloc(sizeof(job_t));
   job->id=job_number;
   job->initialTime=time;
   job->startTime=time;
   job->runningTime=running_time;
   job->priority=priority;
   job->firstResponse=-1;//hasnt entered queue yet

   int i;
   for(i=0;i<numCores;i++){//find idle core
      if(cores[i].isIdle==0){
         job->firstResponse=time;
         cores[i].isIdle=1;
         cores[i].myJob=job;
         return i;
      }
   }

   if(myScheme==FCFS || myScheme==SJF || myScheme==PRI){//add to queue cuz no preemption
      priqueue_offer(priorityQueue,job);
      return -1;
   }
   if(myScheme==PSJF){//find core using job with longest runtime,switch out if that runtime is longer than current job
      int j=-1;
      int biggestTime=-1;
      int i;
      for(i=0;i<numCores;i++){
         double remainingTime=cores[i].myJob->runningTime-(time-cores[i].myJob->startTime);
         if(remainingTime>biggestTime || (remainingTime==biggestTime && cores[i].myJob->initialTime>cores[j].myJob->initialTime)){
            biggestTime=remainingTime;
            j=i;
         }
      }
      if(job->runningTime<biggestTime){//preempt and put one that is running onto queue
         if(cores[j].myJob->firstResponse==time){
            cores[j].myJob->firstResponse=-1;
            totalResponseTime-=cores[j].myJob->actualResponseTime;
         }
         cores[j].myJob->runningTime=cores[j].myJob->runningTime-(time-cores[j].myJob->startTime);
         cores[j].myJob->startTime=time;
         priqueue_offer(priorityQueue,cores[j].myJob);
         cores[j].myJob=job;
         job->firstResponse=time;
         return j;
      }else{//dont preemp and put current job on queue
         priqueue_offer(priorityQueue,job);
         return -1;
      }
   }else if(myScheme==PPRI){//lower number means higher priority
      int j=-1;
      int lowestPriority=-1;
      int i;
      for(i=0;i<numCores;i++){
         if(cores[i].myJob->priority>lowestPriority || (cores[i].myJob->priority==lowestPriority && cores[i].myJob->initialTime>cores[j].myJob->initialTime)){
            lowestPriority=cores[i].myJob->priority;
            j=i;
         }
      }
      if(job->priority<lowestPriority){//preempt and put one that us running onto queue
        if(cores[j].myJob->firstResponse==time){
            cores[j].myJob->firstResponse=-1;
            totalResponseTime-=cores[j].myJob->actualResponseTime;
         }
         cores[j].myJob->runningTime=cores[j].myJob->runningTime-(time-cores[j].myJob->startTime);
         cores[j].myJob->startTime=time;
         priqueue_offer(priorityQueue,cores[j].myJob);
         cores[j].myJob=job;
         job->firstResponse=time;
         return j;
      }else{//dont preemp and put current job on queue
         priqueue_offer(priorityQueue,job);
         return -1;
      }
   }else if(myScheme==RR){
         priqueue_offer(priorityQueue,job);
         return -1;
   }
   
	return -1;
}


/**
  Called when a job has completed execution.
 
  The core_id, job_number and time parameters are provided for convenience. You
  may be able to calculate the values with your own data structure.
  If any job should be scheduled to run on the core free'd up by the
  finished job, return the job_number of the job that should be scheduled to
  run on core core_id.
 
  @param core_id the zero-based index of the core where the job was located.
  @param job_number a globally unique identification number of the job.
  @param time the current time of the simulator.
  @return job_number of the job that should be scheduled to run on core core_id
  @return -1 if core should remain idle.
 */
int scheduler_job_finished(int core_id, int job_number, int time)
{
   job_t *job=cores[core_id].myJob;

  totalTurnaroundTime+=time-job->initialTime;

  free(job);

  if(priqueue_size(priorityQueue)==0){
    cores[core_id].myJob=NULL;
    cores[core_id].isIdle=0;
    return -1;
  }

  job_t *nextJob=priqueue_poll(priorityQueue);
  if(nextJob->firstResponse==-1){
      nextJob->firstResponse=time;//has gone through the queue
      totalResponseTime+=(time-(nextJob->initialTime));
      nextJob->actualResponseTime=time-(nextJob->initialTime);
  }
  totalWaitTime+=(time-(nextJob->startTime));
  nextJob->startTime=time;
  cores[core_id].myJob=nextJob;
  return nextJob->id;
}


/**
  When the scheme is set to RR, called when the quantum timer has expired
  on a core.
 
  If any job should be scheduled to run on the core free'd up by
  the quantum expiration, return the job_number of the job that should be
  scheduled to run on core core_id.

  @param core_id the zero-based index of the core where the quantum has expired.
  @param time the current time of the simulator. 
  @return job_number of the job that should be scheduled on core cord_id
  @return -1 if core should remain idle
 */
int scheduler_quantum_expired(int core_id, int time)
{
   job_t *job=cores[core_id].myJob;
   job->runningTime=job->runningTime-(time-(job->startTime));
   job->startTime=time;
   priqueue_offer(priorityQueue,job);

  if(priqueue_size(priorityQueue)==0){
   cores[core_id].myJob=NULL;
    cores[core_id].isIdle=0;
    return -1;
  }

  job_t *nextJob=priqueue_poll(priorityQueue);
  if(nextJob->firstResponse==-1){
      nextJob->firstResponse=time;
      totalResponseTime+=(time-(nextJob->startTime));
      nextJob->actualResponseTime=time-(nextJob->startTime);
  }
  totalWaitTime+=(time-(nextJob->startTime));
  nextJob->startTime=time;
  cores[core_id].myJob=nextJob;
  return nextJob->id;

}


/**
  Returns the average waiting time of all jobs scheduled by your scheduler.

  Assumptions:
    - This function will only be called after all scheduling is complete (all
      jobs that have arrived will have finished and no new jobs will arrive).
  @return the average waiting time of all jobs scheduled.
 */
float scheduler_average_waiting_time()
{
	return totalWaitTime/numJobs;
}


/**
  Returns the average turnaround time of all jobs scheduled by your scheduler.

  Assumptions:
    - This function will only be called after all scheduling is complete (all
      jobs that have arrived will have finished and no new jobs will arrive).
  @return the average turnaround time of all jobs scheduled.
 */
float scheduler_average_turnaround_time()
{
	return totalTurnaroundTime/numJobs;
}


/**
  Returns the average response time of all jobs scheduled by your scheduler.

  Assumptions:
    - This function will only be called after all scheduling is complete (all
      jobs that have arrived will have finished and no new jobs will arrive).
  @return the average response time of all jobs scheduled.
 */
float scheduler_average_response_time()
{
	return totalResponseTime/numJobs;
}


/**
  Free any memory associated with your scheduler.
 
  Assumptions:
    - This function will be the last function called in your library.
*/
void scheduler_clean_up()
{
  int i;
  free(cores);
  priqueue_destroy(priorityQueue);
  free(priorityQueue);

}


/**
  This function may print out any debugging information you choose. This
  function will be called by the simulator after every call the simulator
  makes to your scheduler.
  In our provided output, we have implemented this function to list the jobs in
  the order they are to be scheduled. Furthermore, we have also listed the
  current state of the job (either running on a given core or idle). For
  example, if we have a non-preemptive algorithm and job(id=4) has began
  running, job(id=2) arrives with a higher priority, and job(id=1) arrives with
  a lower priority, the output in our sample output will be:

    2(-1) 4(0) 1(-1)  
  
  This function is not required and will not be graded. You may leave it blank
  if you do not find it useful.
 */
void scheduler_show_queue()
{
   entry *entry=priorityQueue->head;
   while(entry!=NULL){
      job_t *job=(job_t *)(entry->value);
     // printf("%d(%d)",job->id,job->core);
      entry=entry->next;
   }
	// This function is left entirely to you! Totally optional.
}