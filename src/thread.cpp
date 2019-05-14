/* ***
 * @ $thread.cpp
 * 
 * Copyright (C) 2018 Hsiang Chen
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * ***/
#include <cstring>

#include "thread.h"
#include "utils.h"

Thread::Thread()
{
  if((status = pthread_attr_init(&attr)) != 0) {
    log::erro("Thread:Thread:pthread_attr_init()");
  }
}

Thread::~Thread()
{
  kill_threads();

  if (! status) {
    pthread_attr_destroy(&attr);
  }
}

int Thread::create_thread(void* (*start_routine)(void*), void* args, pthread_t& id)
{
  if (status) return -1;

  if (args == NULL) return -1;

  int rev = -1;
  pthread_t pid;

  if (! (rev = pthread_create(&pid, &attr, start_routine, args))) {
    pt_map.insert(make_pair(id = pid, args));
  }

  return rev;
}

void Thread::kill_threads(void)
{
  map<pthread_t, void*>::iterator it, end = pt_map.end();
  
  while ((it = pt_map.begin()) != end) {
    kill_thread(it->first);
  }
}

void Thread::kill_thread(pthread_t td)
{
  thread_del(td, true);
}

void Thread::join_threads(void)
{
  map<pthread_t, void*>::iterator it, end = pt_map.end();

  while ((it = pt_map.begin()) != end) {
    join_thread(it->first);
  }
}

void Thread::join_thread(pthread_t td)
{
  void* result = NULL;

  pthread_join(td, &result);
  thread_del(td, false);
}

size_t Thread::threads_count(void)
{
  return pt_map.size();
}

pair<pthread_t, void*> Thread::thread_get(size_t index)
{
  map<pthread_t, void*>::iterator it = pt_map.begin();

  size_t i; for (i = 0; i < index; i++) it++;

  if (it != pt_map.end()) {
    return make_pair(it->first, it->second);
  } else {
    return pair<pthread_t, void*>(0, NULL);
  }
}

void Thread::thread_del(pthread_t td, bool kill)
{
  map<pthread_t, void*>::iterator it = pt_map.find(td);

  if (it != pt_map.end()) {
    if (kill) pthread_kill(td, 0);
    pt_map.erase(it);
  }
}

/*end*/
