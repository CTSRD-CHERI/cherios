/*
   wqueue.h
   Worker thread queue based on the Standard C++ library list
   template class.
   ------------------------------------------
   Copyright (c) 2013 Vic Hargrave
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at
       http://www.apache.org/licenses/LICENSE-2.0
   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

// https://vichargrave.github.io/articles/2013-01/multithreaded-work-queue-in-cpp
// https://github.com/vichargrave/wqueue/blob/master/wqueue.h


#ifndef __wqueue_h__
#define __wqueue_h__

#include <pthread.h>
#include <list>
#include <spinlock.h>
#include <condition.h>

using namespace std;

//TODORM:
#include <cheristd.h>
#include <colors.h>

template <typename T> class wqueue
{
    list<T>              m_queue;
    spinlock_t           m_mutex;
    //pthread_mutexattr_t  m_mutex_attr;
    //pthread_cond_t       m_condv;
    //condition_t          m_condv;

public:
    wqueue()
    {
        //pthread_mutexattr_init(&m_mutex_attr);
        //pthread_mutexattr_settype(&m_mutex_attr, PTHREAD_MUTEX_RECURSIVE);
        //pthread_mutex_init(&m_mutex, &m_mutex_attr);
        //pthread_cond_init(&m_condv, NULL);
        spinlock_init(&m_mutex);
        //m_condv = 0;
    }

    ~wqueue() {
        //pthread_mutex_destroy(&m_mutex);
        //pthread_cond_destroy(&m_condv);
    }

    void put(T item) {
        spinlock_acquire(&m_mutex);
        m_queue.push_back(item);
        //pthread_cond_signal(&m_condv);
        //condition_set_and_notify(m_condv, 1);
        spinlock_release(&m_mutex);
    }

    T get(uint32_t tmo) {
        spinlock_acquire(&m_mutex);
        if (tmo > 0) {
            while (m_queue.size() == 0) {
                //pthread_cond_wait(&m_condv, &m_mutex);
                spinlock_release(&m_mutex);
                //printf(KRED"%s SLEEPIN %zu\n" KRST, __func__, blah);
                //sleep(MS_TO_CLOCK(1));
                for (volatile int i=0; i<100000; i++);
                //printf(KRED"%s SLEEPOUT %zu\n" KRST, __func__, blah++);
                spinlock_acquire(&m_mutex);
            }
        }

        T item = NULL;

        if (m_queue.size() != 0) {
            item = m_queue.front();
            m_queue.pop_front();
        }

        spinlock_release(&m_mutex);
        return item;
    }

    void remove(T item) {
        spinlock_acquire(&m_mutex);
        m_queue.remove(item);
        spinlock_release(&m_mutex);
    }

    int size() {
        spinlock_acquire(&m_mutex);
        int size = m_queue.size();
        spinlock_release(&m_mutex);
        return size;
    }
};

#endif
