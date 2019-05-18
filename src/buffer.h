/* $ @buffer.h
 * Copyright (C) 2018 Hsiang Chen
 * This software is free software,you can redistributed in the term of GNU Public License.
 * For detail see <http://www.gnu.org/licenses>
 * This file is autogenated by perl script. DO NOT edit it.
 * */
#ifndef	_BUFFER_H_
#define	_BUFFER_H_

#include <ostream>

using namespace std;

class Buffer {
public:
  Buffer();
  Buffer(const Buffer& buf);
  ~Buffer();

  bool    alloc(size_t len);
  bool    app(const void* mem, size_t len);
  void    reset(void);
  void    resize(size_t len);
  void*   ptr(void) const;
  size_t  size(void) const;
  size_t  capacity(void) const;

  Buffer& operator += (const Buffer& buf);
  char&   operator [] (size_t ix);
  Buffer& operator =  (const Buffer& buf);
private:
  bool    invalid;
  char*   mptr;
  char*   mpt;
  size_t  len_total;
  size_t  len_used;
};

#endif	/* _BUFFER_H_ */
