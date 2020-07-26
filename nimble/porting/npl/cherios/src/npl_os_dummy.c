/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <stddef.h>
#include "nimble/nimble_npl.h"

void *
ble_npl_get_current_task_id(void)
{
    return NULL;
}

/* Horrible, but anyway... */
ble_npl_error_t
ble_npl_sem_init(struct ble_npl_sem *sem, uint16_t tokens)
{
    if (tokens != 0) {
        return BLE_NPL_ENOENT;
    }
    sem->count = 0;
    return ble_npl_mutex_init(&sem->mu);
}

ble_npl_error_t
ble_npl_sem_pend(struct ble_npl_sem *sem, ble_npl_time_t timeout)
{
    /* Poll */
    ble_npl_time_t elapsed = 0;
    while (elapsed < timeout) {
        ble_npl_mutex_pend(&sem->mu, BLE_NPL_TIME_FOREVER);
        int count = sem->count;
        if (count > 0) {
            sem->count--;
            ble_npl_mutex_release(&sem->mu);
            break;
        }
        ble_npl_mutex_release(&sem->mu);
        ble_npl_time_t sleep_chunk_ms = 10;
        elapsed += sleep_chunk_ms;
        sleep(MS_TO_CLOCK(sleep_chunk_ms));
    }
    if (elapsed >= timeout) {
        return BLE_NPL_TIMEOUT;
    }
    return BLE_NPL_OK;
}

ble_npl_error_t
ble_npl_sem_release(struct ble_npl_sem *sem)
{
    ble_npl_mutex_pend(&sem->mu, BLE_NPL_TIME_FOREVER);
    sem->count++;
    ble_npl_mutex_release(&sem->mu);
    // No need to wake-up people on 'pend', they are doing polling.
    return BLE_NPL_OK;
}

uint16_t
ble_npl_sem_get_count(struct ble_npl_sem *sem)
{
    ble_npl_mutex_pend(&sem->mu, BLE_NPL_TIME_FOREVER);
    uint16_t count = sem->count;
    ble_npl_mutex_release(&sem->mu);
    return count;
}

#if 1
void
ble_npl_callout_init(struct ble_npl_callout *c, struct ble_npl_eventq *evq,
                     ble_npl_event_fn *ev_cb, void *ev_arg)
{ //TODO: this is used!!
}

ble_npl_error_t
ble_npl_callout_reset(struct ble_npl_callout *c, ble_npl_time_t ticks)
{
    return BLE_NPL_ENOENT; //TODO: this is used!!
}

void
ble_npl_callout_stop(struct ble_npl_callout *co)
{
 //TODO: this is used!!
}

bool
ble_npl_callout_is_active(struct ble_npl_callout *c)
{
    return false; //TODO: this is used!!
}

ble_npl_time_t
ble_npl_callout_get_ticks(struct ble_npl_callout *co)
{
    return 0; //TODO: this is used!!
}
#endif

uint32_t
ble_npl_time_get(void)
{
    return 0; //TODO: this is used!!
}


/* Simple 1:1. We handle the tick conversion when needed in lower-level code */
ble_npl_error_t
ble_npl_time_ms_to_ticks(uint32_t ms, ble_npl_time_t *out_ticks)
{
    *out_ticks = ms;
    return BLE_NPL_OK;
}

ble_npl_error_t
ble_npl_time_ticks_to_ms(ble_npl_time_t ticks, uint32_t *out_ms)
{
    *out_ms = ticks;
    return BLE_NPL_OK;
}

ble_npl_time_t
ble_npl_time_ms_to_ticks32(uint32_t ms)
{
    return ms;
}

uint32_t
ble_npl_time_ticks_to_ms32(ble_npl_time_t ticks)
{
    return ticks;
}

uint32_t
ble_npl_hw_enter_critical(void)
{
    return 0; //TODO: this is used!!
}

void
ble_npl_hw_exit_critical(uint32_t ctx)
{
 //TODO: this is used!!
}
