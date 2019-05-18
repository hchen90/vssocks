/* ***
 * @ $buffer.c
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

#include "config.h"
#include "buffer.h"

#include "utils.h"

using namespace std;

/*class: Buffer*/

Buffer::Buffer() : invalid(false), mptr(NULL), mpt(NULL), len_total(BUFF_SIZE), len_used(0)
{
  mptr = mpt = new char[len_total];
}

Buffer::Buffer(const Buffer& buf) : Buffer()
{
  if (alloc(buf.capacity())) {
    app(buf.ptr(), buf.size());
  }
}

Buffer::~Buffer()
{
  if (mptr != NULL) {
    delete[] mptr;
    mptr = mpt = NULL;
  }
}

bool Buffer::alloc(size_t len)
{
  if (len > len_total) {
    delete[] mptr;
    len_total = len * 2;
    mptr = new char[len_total];
    reset();
  }

  return mptr != NULL;
}

bool Buffer::app(const void* mem, size_t len)
{
  if (len + len_used > len_total) {
    size_t l = len * 2;
    char* p = new char[l];

    invalid = true;

    if (p != NULL) {
      memcpy(p, mptr, len_total);
      len_total = l;
      delete[] mptr;
      mptr = p; // update `mptr' and `mpt'
      mpt = p + len_used;
      invalid = false;
    }
  }

  if (! invalid && mpt != NULL) {
    memcpy(mpt, mem, len);
    mpt += len;
    len_used += len;
    return true;
  }

  return false;
}

void Buffer::reset(void)
{
  mpt = mptr;
  len_used = 0;
}

void Buffer::resize(size_t len)
{
  if (len <= len_total) {
    len_used = len;
  }
}

void* Buffer::ptr(void) const
{
  return mptr;
}

size_t Buffer::size(void) const
{
  return len_used;
}

size_t Buffer::capacity(void) const
{
  return len_total;
}

Buffer& Buffer::operator += (const Buffer& buf)
{
  void* p = buf.ptr(); size_t l = buf.size();

  if (p != NULL && l > 0) {
    app(p, l);
  }

  return *this;
}

char& Buffer::operator [] (size_t ix)
{
  if (mptr != NULL) {
    return mptr[ix];
  } else {
    return mptr[0];
  }
}

Buffer& Buffer::operator = (const Buffer& buf)
{
  invalid   = buf.invalid;
  len_total = buf.len_total;
  len_used  = buf.len_used;

  if (mptr != NULL) {
    delete mptr;
  }

  mptr      = new char[len_total];
  mpt       = mptr + len_used;

  if (mptr != NULL) {
    memcpy(mptr, buf.mptr, len_total);
  }

  return *this;
}

/*end*/
