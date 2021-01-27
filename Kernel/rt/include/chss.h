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

/**
 * @file    chss.h
 * @brief   Sporadic Server  Module macros and structures.
 *
 * @addtogroup Sporadic Server
 * @{
 */
#ifndef CHSS_H
#define CHSS_H

#include "ch.h"

#if CH_CFG_USE_SS

/*===========================================================================*/
/* Module constants.                                                         */
/*===========================================================================*/
/*
 * @name Capacity Helper
 * @{
 */
#define TICK_BOUND 4294967295 /**<@brief:2^32-1, used to check if we are in a new clock cycle.  */
/** @} */
/*
 * @name Replinishment constants
 * @{
 */
#define NUM_REP 256 /*<@brief number of replinishments in a replinishment array.    */
#define NUM_TIM 10  /*<@brief number of timer that can be armed to manage replinishments.   */
/** @} */
/*
 * @name Sporadic Thd Constants
 * @{
 */
#define SPORADIC_WA 256/*<@brief working area of the sporadic Server*/
/** @} */
/*
 * @name Sporadic Thd dbg
 * @{
 */
#define SPORADIC_DBG 256/*<@brief Sporadic DBG flg*/
/** @} */

/*===========================================================================*/
/* Module pre-compile time settings.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/


/*===========================================================================*/
/* Module data structures and types.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Module macros.                                                            */
/*===========================================================================*/

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif

  void __sporadicserver_updatetime(const thread_t*,const thread_t*);
  thread_t* chSporadicServerObjectInit(systime_t ,systime_t,tprio_t);
  AperiodicRequest* chSporadicServerCreateAperiodic(void* ,void*,AperiodicRequest*);
  AperiodicRequest* chSporadicServerAperiodicQueueInsert(AperiodicRequest*);
  AperiodicRequest* chSporadicServerAperiodicQueueInsertS(AperiodicRequest*);
  bool chSporadicServerNeedWU(void);
  thread_t* chSporadicServerGetInstance(void);
#if SPORADIC_DBG
  void chSporadicServerGetTime(uint32_t*,uint32_t*,uint32_t*);
#endif
#ifdef __cplusplus
}
#endif

/*===========================================================================*/
/* Module inline functions.                                                  */
/*===========================================================================*/

#endif/* CH_CFG_USE_SS */
#endif/*CHSS_H*/
/** @} */
