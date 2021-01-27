/*
    ChibiOS - Copyright (C) 2006..2018 Giovanni Di Sirio.
    This file is part of ChibiOS.
    ChibiOS is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.
    ChibiOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/*
    This file is not part of the original ChibiOs 191.
    Copyright (C) 2021 Antonio Emmanuele.
*/
/**
 * @file    chss.c
 * @brief   Sporadic Server module code.
 *
 * @addtogroup Sporadic Server
 * @{
 */

#include "chss.h"

#if (CH_CFG_USE_SS==TRUE)
/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

/*===========================================================================*/
/* Module exported variables.                                                */
/*===========================================================================*/

/*===========================================================================*/
/* Module local types.                                                       */
/*===========================================================================*/

/*===========================================================================*/
/* Module local variables.                                                   */
/*===========================================================================*/

/*
 * @brief   Sporadic Server Reference
 */
static thread_reference_t sporadic;
/*
 * @brief   array of capacity replinishments
 */
static uint32_t replinishment_array[NUM_REP];
/*
 * @brief   Bitmap of the replinishment array
 */
static uint32_t repArrBTM[NUM_REP];
/*
 * @brief   Array of VT used to call the replinishment cb
 */
static virtual_timer_t rep_vt[NUM_TIM];
/*
 * @brief   Virtual timer that calls the reservation CB
 */
static virtual_timer_t reservation_vt;
/*
 * @brief   Sporadic Server Thd Working area
 */
static THD_WORKING_AREA(waSporadicServer,SPORADIC_WA);
#if SPORADIC_DBG
  static uint32_t dbg_arr[10]={1,1,1,1,1,1,1,1,1,1};
  static uint8_t dbg_arr_index=0;
  static uint32_t num_called=0;
#endif
/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/
/*
 * @brief   Sporadic Server Thd
 */
static THD_FUNCTION(SporadicServer,args){
  (void)args;
  while (true){
    /*executes the first function in the aperiodic quque*/
    (void)(*sporadic->requests->fun_ptr)(sporadic->requests->arg);
    /*update the queue*/
    sporadic->requests=sporadic->requests->next;
    /*suspends the sporadic server,must add a controll on thread priority , SchGoSleepS sets the new state, takes the next task from the ready list(setting it as current) and performs a context switch*/
    if((sporadic->requests==0) || firstprio(&ch.rlist.queue)>sporadic->prio){
      if(sporadic->requests==0)
        chSchGoSleepS(CH_STATE_SLEEPING);
      else if(firstprio(&ch.rlist.queue)>sporadic->prio){
        chSchReadyI(sporadic);
        chSchGoSleepS(CH_STATE_READY);
      }
    }
  }
}
/*
 * @brief   find the index of the first free rp_vt
 * @pre     there must be a free vt, and the virtual timer array should contain less than 256 elements(in this case we won't know if it hasn't find the VT or it is the last)
 * @ret     returns the index of the free vt in the rep_vt array, if no free vt returns 255
 */
static uint8_t  __get_freeVT(void){
  for(uint8_t i=0;i<NUM_TIM;i++){
    if(!chVTIsArmedI(&rep_vt[i]))
      return i;
  }
  return 255;
}

/*
 * @brief   inits the replinishment array and his bitmap
 * @pre     rep array shouldn't have been initialized
 * @post    rep array has been initialized and the bitman is composed from all 0(meaning all slots are free)
 */
static void __repArrInit(void){
  for(uint32_t i=0;i<NUM_REP;i++){
    replinishment_array[i]=0;
    repArrBTM[i]=0;
  }
}
/*
 * @brief   inserts a capacity in the replinishment array
 * @pre:    here should be a free slot to insert the replinishment, else it overwrites the last(bad solution)
 * @post    the next replinishment is inserted in the first available slot
 */
static void __repArrInsert(uint32_t val){
  bool cond=true;
  uint32_t i=0;
  /*find the first replinishment slot available*/
  while(i<NUM_REP && cond){
   if(repArrBTM[i]==0) {
     replinishment_array[i]=val;
     repArrBTM[i]=1;
     cond=false;
   }
   else
     i++;
  }
  /*if it didn't find an element,maybe there is a problem, overwrite the last*/
  if(cond){
    replinishment_array[NUM_REP-1]=val;
    repArrBTM[NUM_REP-1]=1;
  }
}
/*
 * @brief   Remove a capacity from the replinishment array
 * @pre     there should be at least one replinishment
 * @post    the first replinishment found is now free
 * @ret     0 if not found, else the capacity to be replinished
 */
static uint32_t __repArrRemove(void){
  for(uint32_t i=0;i<32;i++){
    if(repArrBTM[i]==1){
      repArrBTM[i]=0;
      return replinishment_array[i];
    }
  }
  return 0;
}
#if SPORADIC_DBG
static void __dbgArrInsert(uint32_t val){
  if(dbg_arr_index<10)
     dbg_arr[dbg_arr_index]=val;
  else{
    dbg_arr_index=0;
    dbg_arr[dbg_arr_index]=val;
  }
  dbg_arr_index++;
}
#endif
/*
 *@brief    CB of the sporadic server replinishment
 *@pre      there must be at least one replinishment
 *@post     there is a new free slot in replinishment array and the capacity has been updated
 */
static void __SporadicServerReplinishmentCB(void*arg){
  (void)arg;
  uint32_t old=sporadic->capacity;
#if SPORADIC_DBG
  uint32_t rep=__repArrRemove();
  __dbgArrInsert(rep);
  sporadic->capacity+=rep;
#elif
  sporadic->capacity+=__repArrRemove();
#endif
  /*
   * Maybe unnecessary, but better be sure :)
   */
  if(sporadic->capacity>sporadic->maximum_capacity)
    sporadic->capacity=sporadic->maximum_capacity;
  /*if the sporadic was suspended, means that his state was suspended and old capacity was 0
  * Also we must place it in the ready list if it has a pending request, if not we will insert it in the ready list and when the
  * first request will come CORRUPTION(of the rlist)
  */
  if(sporadic->state==CH_STATE_SUSPENDED&&old==0&&sporadic->requests!=0)
    chSchReadyI(sporadic);
}
/*
 * @brief   Time reservation CallBack
 * @note    Set ending=true, after this the IsPreemptionRequired function called going out from this VT will set SS as Suspended and it will be preempted
 */
static void __SporadicServerReservationCB(void*args){
  (void)args;
  sporadic->ending=true;
  num_called++;
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/*
 * @brief   Context Switch hook for the sporadic server
 * @note    The first part of the function (195-220) calculates the consumed time and checks if the Sporadic Exceed, the second (221-243) manages TA and the replinishments.
 * @pre     there must be a sporadic server active
 * @post    in case we are the otp the capacity has been updated, if Pexe<Psporadic ||Cs==0 a timer is armed
 */
void __sporadicserver_updatetime(const thread_t*ntp,const thread_t*otp){
  /*if the sporadic server is entering the cpu*/
  if(ntp==sporadic){
    sporadic->instance_start=chVTGetSystemTimeX();
    chVTDoSetI(&reservation_vt,sporadic->capacity,__SporadicServerReservationCB,NULL);
  }
  else if(otp==sporadic){
    /*  if the sporadic server is leaving cpu */
    if(chVTIsArmed(&reservation_vt))
      chVTDoResetI(&reservation_vt);
    sporadic->instance_end=chVTGetSystemTimeX();
    /*
     * if we are in a new Clock cycle
     */
    if(sporadic->instance_start>sporadic->instance_end)
      sporadic->consumed_time +=chTimeAddX(chTimeDiffX(sporadic->instance_start,TICK_BOUND),sporadic->instance_end);
    else
      sporadic->consumed_time += chTimeDiffX(sporadic->instance_start,sporadic->instance_end);
    /* Capacity update part */
    if(sporadic->consumed_time>=sporadic->capacity)
      sporadic->capacity=0;
    else
      sporadic->capacity=sporadic->capacity -  sporadic->consumed_time;
    /*updating to replinish */
    sporadic->timeToReplinish +=sporadic->consumed_time;
    sporadic->consumed_time=0;
  }
  if(sporadic->capacity>0 && (ntp->prio)>=sporadic->prio && sporadic->mustUpdateTA){
     sporadic->TA=chVTGetSystemTimeX();
     /*Guard variable*/
     sporadic->mustUpdateTA=false;
   }
  else if((!((ntp->prio)>=sporadic->prio)||sporadic->capacity==0)&&(sporadic->mustUpdateTA==false)){
    sporadic->mustUpdateTA=true;
    /*Removing the sporadic from the ready list*/
    if(sporadic->capacity==0 && otp==sporadic){
      if(sporadic->state==CH_STATE_READY){
        sporadic->queue.prev->queue.next=sporadic->queue.next;
        sporadic->queue.next->queue.prev=sporadic->queue.prev;
      }
      sporadic->state=CH_STATE_SUSPENDED;
    }
    if(sporadic->timeToReplinish!=0){
      __repArrInsert(sporadic->timeToReplinish);
      sporadic->timeToReplinish=0;
      chVTDoSetI(&rep_vt[ __get_freeVT()],(sporadic->TA+sporadic->period)-chVTGetSystemTimeX(),__SporadicServerReplinishmentCB,NULL);

    }
  }
}
/*
 * @brief   Inits (the only one ) sporadic server
 * @par_in  period of the sporadic server,capacity of the sporadic server , priority of the sporadic server
 * @pre     This fun must be called only once
 * @post    The Sporadic Server Thd will be initialized
 * @ret     Sporadic Server thd pointer
 */
thread_t* chSporadicServerObjectInit(systime_t period ,systime_t capacity,tprio_t priority){
  thread_t* td;
  chSysLock();
  /* The thread structure is laid out in the upper part of the thread
     workspace. The thread position structure is aligned to the required
     stack alignment because it represents the stack top.*/
  td = (thread_t *)((uint8_t *)waSporadicServer + sizeof(waSporadicServer) -
                    MEM_ALIGN_NEXT(sizeof (thread_t), PORT_STACK_ALIGN));

  PORT_SETUP_CONTEXT(td, waSporadicServer, td, SporadicServer, NULL);

  td = _thread_init(td, "noname", priority);
  td->period=period;
  td->capacity=capacity;
  td->maximum_capacity=capacity;
  td->instance_start=0;
  td->instance_end=0;
  td->consumed_time=0;
  td->TA=0;
  td->requests=0;
  td->mustUpdateTA=true;
  td->timeToReplinish=0;
  sporadic=td;
  __repArrInit();
  chSysUnlock();
  return td;
}

/*
 *@brief    Inserts an aperiodic request in the aperiodic request queue and returns the pointer
 *@note     If it is the first request then it updates the sporadic
 *@pre      the ap req should have been initialized previously
 *@post     a new ap req in the queue of the sporadic
 *@ret      pointer to the ap req
 *@S class api
 */
AperiodicRequest* chSporadicServerAperiodicQueueInsertS(AperiodicRequest*ap){
  /*
   * If it is the first
   */
  if(sporadic->requests==0){
    sporadic->requests=ap;
    sporadic->requests->next=0;
    chSchReadyI(sporadic);
  }
  /* else inserts it in the tail of the queue*/
  else{
    AperiodicRequest *app=sporadic->requests;
    while(app->next!=0)
      app=app->next;
    app->next=ap;
    ap->next=0;
  }
  return ap;
}

/*
 * @brief   Initialize and insert an aperiodic request in the queue and returns his pointer
 * @pre     the aperiodic request shouldn't have been initialized yet
 * @post    there is one more aperiodic request in the aperiodic request queue
 * @ret     pointer to the aperiodic request
 */
AperiodicRequest* chSporadicServerCreateAperiodic(void*fun,void*arg,AperiodicRequest*ap){
  chSysLock();
  ap->fun_ptr=fun;
  ap->arg=arg;
  ap=chSporadicServerAperiodicQueueInsertS(ap);
  chSysUnlock();
  return ap;
}

/*
 *@brief    Inserts an aperiodic request in the aperiodic request queue and returns the pointer
 *@note     If it is the first request then it updates the sporadic
 *@pre      the ap req should have been initialized previously
 *@post     a new ap req in the queue of the sporadic
 *@ret      pointer to the ap req
 */
AperiodicRequest*chSporadicServerAperiodicQueueInsert(AperiodicRequest*ap){
  chSysLock();
  ap=chSporadicServerAperiodicQueueInsertS(ap);
  chSysUnlock();
  return ap;
}

/*
 * @brief   Used to check if the SS need to be woke up
 * @ret     True is the state is different fromm CH_STATE_READY
 */
bool chSporadicServerNeedWU(void){
  return sporadic->state!=CH_STATE_READY;
}

/*
 * @brief   Returns a pointer to the Sporadic Server thd
 * @ret     Sporadic Server Thread Pointer
 */
thread_t* chSporadicServerGetInstance(void){
  return sporadic;
}
#if SPORADIC_DBG

/*
 * @brief   Obtain a vector of last timeConsumed
 * @out Number of element in the array and the array
 *
 */
void chSporadicServerGetTime(uint32_t*num,uint32_t*arr,uint32_t* called_time){
  *num=dbg_arr_index;
  for(uint8_t i=0;i<*num;i++)
    arr[i]=dbg_arr[i];
  *called_time=num_called;
}
#endif
#endif/*CH_CFG_USE_SS*/
/** @} */
