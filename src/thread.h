/* $ @thread.h
 * Copyright (C) 2018 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * */
#ifndef	_THREAD_H_
#define	_THREAD_H_

#include <signal.h>
#include <pthread.h>

#include <map>
#include <string>

using namespace std;

class Thread {
public:
  Thread();
  ~Thread();

  int     create_thread(void* (* start_routine)(void*), void* args);
  void    kill_threads(void);
  void    kill_thread(pthread_t td);
  void    join_threads(void);
  void    join_thread(pthread_t td);
  size_t  threads_count(void);
  pair<pthread_t, void*>  thread_get(size_t index);
  void    thread_del(pthread_t td);
private:
  int     status;
  pthread_attr_t attr;

  map<pthread_t, void*> pt_map;
};


#endif	/* _THREAD_H_ */
