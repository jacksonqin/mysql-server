/*
   Copyright (c) 2003-2006, 2008, 2013, Oracle and/or its affiliates. All 
   rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */


#ifndef DL_HASHTABLE_HPP
#define DL_HASHTABLE_HPP

#include <ndb_global.h>
#include "ArrayPool.hpp"

#define JAM_FILE_ID 313


/**
 * DLMHashTable implements a hashtable using chaining
 *   (with a double linked list)
 *
 * The entries in the (uninstansiated) meta class passed to the
 * hashtable must have the following methods:
 *
 *  -# nextHash(T&) returning a reference to the next link
 *  -# prevHash(T&) returning a reference to the prev link
 *  -# bool equal(T const&,T const&) returning equality of the objects keys
 *  -# hashValue(T) calculating the hash value
 */

template <typename T, typename U = T> struct DLHashTableDefaultMethods {
static Uint32& nextHash(U& t) { return t.nextHash; }
static Uint32& prevHash(U& t) { return t.prevHash; }
static Uint32 hashValue(T const& t) { return t.hashValue(); }
static bool equal(T const& lhs, T const& rhs) { return lhs.equal(rhs); }
};

template <typename P, typename T, typename M = DLHashTableDefaultMethods<T> >
class DLMHashTable
{
public:
  explicit DLMHashTable(P & thePool);
  ~DLMHashTable();
private:
  DLMHashTable(const DLMHashTable&);
  DLMHashTable&  operator=(const DLMHashTable&);

public:
  /**
   * Set the no of bucket in the hashtable
   *
   * Note, can currently only be called once
   */
  bool setSize(Uint32 noOfElements);

  /**
   * Seize element from pool - return i
   *
   * Note *must* be added using <b>add</b> (even before hash.release)
   *             or be released using pool
   */
  bool seize(Ptr<T> &);

  /**
   * Add an object to the hashtable
   */
  void add(Ptr<T> &);

  /**
   * Find element key in hashtable update Ptr (i & p)
   *   (using key.equal(...))
   * @return true if found and false otherwise
   */
  bool find(Ptr<T> &, const T & key) const;

  /**
   * Update i & p value according to <b>i</b>
   */
  void getPtr(Ptr<T> &, Uint32 i) const;

  /**
   * Get element using ptr.i (update ptr.p)
   */
  void getPtr(Ptr<T> &) const;

  /**
   * Get P value for i
   */
  T * getPtr(Uint32 i) const;

  /**
   * Remove element (and set Ptr to removed element)
   * Note does not return to pool
   */
  void remove(Ptr<T> &, const T & key);

  /**
   * Remove element
   * Note does not return to pool
   */
  void remove(Uint32 i);

  /**
   * Remove element
   * Note does not return to pool
   */
  void remove(Ptr<T> &);

  /**
   * Remove all elements, but dont return them to pool
   */
  void removeAll();

  /**
   * Remove element and return to pool
   */
  void release(Uint32 i);

  /**
   * Remove element and return to pool
   */
  void release(Ptr<T> &);

  class Iterator {
  public:
    Ptr<T> curr;
    Uint32 bucket;
    inline bool isNull() const { return curr.isNull();}
    inline void setNull() { curr.setNull(); }
  };

  /**
   * Sets curr.p according to curr.i
   */
  void getPtr(Iterator & iter) const ;

  /**
   * First element in bucket
   */
  bool first(Iterator & iter) const;

  /**
   * Next Element
   *
   * param iter - A "fully set" iterator
   */
  bool next(Iterator & iter) const;

  /**
   * Get next element starting from bucket
   *
   * @param bucket - Which bucket to start from
   * @param iter - An "uninitialized" iterator
   */
  bool next(Uint32 bucket, Iterator & iter) const;

private:
  Uint32 mask;
  Uint32 * hashValues;
  P & thePool;
};

template <typename P, typename T, typename M>
inline
DLMHashTable<P, T, M>::DLMHashTable(P & _pool)
  : thePool(_pool)
{
  // Require user defined constructor on T since we fiddle
  // with T's members
  ASSERT_TYPE_HAS_CONSTRUCTOR(T);

  mask = 0;
  hashValues = 0;
}

template <typename P, typename T, typename M>
inline
DLMHashTable<P, T, M>::~DLMHashTable()
{
  if (hashValues != 0)
    delete [] hashValues;
}

template <typename P, typename T, typename M>
inline
bool
DLMHashTable<P, T, M>::setSize(Uint32 size)
{
  Uint32 i = 1;
  while (i < size) i *= 2;

  if (mask == (i - 1))
  {
    /**
     * The size is already set to <b>size</b>
     */
    return true;
  }

  if (mask != 0)
  {
    /**
     * The mask is already set
     */
    return false;
  }

  mask = (i - 1);
  hashValues = new Uint32[i];
  for (Uint32 j = 0; j<i; j++)
    hashValues[j] = RNIL;

  return true;
}

template <typename P, typename T, typename M>
inline
void
DLMHashTable<P, T, M>::add(Ptr<T> & obj)
{
  const Uint32 hv = M::hashValue(*obj.p) & mask;
  const Uint32 i  = hashValues[hv];

  if (i == RNIL)
  {
    hashValues[hv] = obj.i;
    M::nextHash(*obj.p) = RNIL;
    M::prevHash(*obj.p) = RNIL;
  }
  else
  {
    T * tmp = thePool.getPtr(i);
    M::prevHash(*tmp) = obj.i;
    M::nextHash(*obj.p) = i;
    M::prevHash(*obj.p) = RNIL;

    hashValues[hv] = obj.i;
  }
}

/**
 * First element
 */
template <typename P, typename T, typename M>
inline
bool
DLMHashTable<P, T, M>::first(Iterator & iter) const
{
  Uint32 i = 0;
  while (i <= mask && hashValues[i] == RNIL) i++;
  if (i <= mask)
  {
    iter.bucket = i;
    iter.curr.i = hashValues[i];
    iter.curr.p = thePool.getPtr(iter.curr.i);
    return true;
  }
  else
  {
    iter.curr.i = RNIL;
  }
  return false;
}

template <typename P, typename T, typename M>
inline
bool
DLMHashTable<P, T, M>::next(Iterator & iter) const
{
  if (M::nextHash(*iter.curr.p) == RNIL)
  {
    Uint32 i = iter.bucket + 1;
    while (i <= mask && hashValues[i] == RNIL) i++;
    if (i <= mask)
    {
      iter.bucket = i;
      iter.curr.i = hashValues[i];
      iter.curr.p = thePool.getPtr(iter.curr.i);
      return true;
    }
    else
    {
      iter.curr.i = RNIL;
      return false;
    }
  }

  iter.curr.i = M::nextHash(*iter.curr.p);
  iter.curr.p = thePool.getPtr(iter.curr.i);
  return true;
}

template <typename P, typename T, typename M>
inline
void
DLMHashTable<P, T, M>::remove(Ptr<T> & ptr, const T & key)
{
  const Uint32 hv = M::hashValue(key) & mask;

  Uint32 i;
  T * p;
  Ptr<T> prev;
  LINT_INIT(prev.p);
  prev.i = RNIL;

  i = hashValues[hv];
  while (i != RNIL)
  {
    p = thePool.getPtr(i);
    if (M::equal(key, * p))
    {
      const Uint32 next = M::nextHash(*p);
      if (prev.i == RNIL)
      {
        hashValues[hv] = next;
      }
      else
      {
        M::nextHash(*prev.p) = next;
      }

      if (next != RNIL)
      {
        T * nextP = thePool.getPtr(next);
        M::prevHash(*nextP) = prev.i;
      }

      ptr.i = i;
      ptr.p = p;
      return;
    }
    prev.p = p;
    prev.i = i;
    i = M::nextHash(*p);
  }
  ptr.i = RNIL;
}

template <typename P, typename T, typename M>
inline
void
DLMHashTable<P, T, M>::remove(Uint32 i)
{
  Ptr<T> tmp;
  tmp.i = i;
  tmp.p = thePool.getPtr(i);
  remove(tmp);
}

template <typename P, typename T, typename M>
inline
void
DLMHashTable<P, T, M>::release(Uint32 i)
{
  Ptr<T> tmp;
  tmp.i = i;
  tmp.p = thePool.getPtr(i);
  release(tmp);
}

template <typename P, typename T, typename M>
inline
void
DLMHashTable<P, T, M>::remove(Ptr<T> & ptr)
{
  const Uint32 next = M::nextHash(*ptr.p);
  const Uint32 prev = M::prevHash(*ptr.p);

  if (prev != RNIL)
  {
    T * prevP = thePool.getPtr(prev);
    M::nextHash(*prevP) = next;
  }
  else
  {
    const Uint32 hv = M::hashValue(*ptr.p) & mask;
    if (hashValues[hv] == ptr.i)
    {
      hashValues[hv] = next;
    }
    else
    {
      // Will add assert in 5.1
      assert(false);
    }
  }

  if (next != RNIL)
  {
    T * nextP = thePool.getPtr(next);
    M::prevHash(*nextP) = prev;
  }
}

template <typename P, typename T, typename M>
inline
void
DLMHashTable<P, T, M>::release(Ptr<T> & ptr)
{
  const Uint32 next = M::nextHash(*ptr.p);
  const Uint32 prev = M::prevHash(*ptr.p);

  if (prev != RNIL)
  {
    T * prevP = thePool.getPtr(prev);
    M::nextHash(*prevP) = next;
  }
  else
  {
    const Uint32 hv = M::hashValue(*ptr.p) & mask;
    if (hashValues[hv] == ptr.i)
    {
      hashValues[hv] = next;
    }
    else
    {
      assert(false);
      // Will add assert in 5.1
    }
  }

  if (next != RNIL)
  {
    T * nextP = thePool.getPtr(next);
    M::prevHash(*nextP) = prev;
  }

  thePool.release(ptr);
}

template <typename P, typename T, typename M>
inline
void
DLMHashTable<P, T, M>::removeAll()
{
  for (Uint32 i = 0; i<=mask; i++)
    hashValues[i] = RNIL;
}

template <typename P, typename T, typename M>
inline
bool
DLMHashTable<P, T, M>::next(Uint32 bucket, Iterator & iter) const
{
  while (bucket <= mask && hashValues[bucket] == RNIL)
    bucket++;

  if (bucket > mask)
  {
    iter.bucket = bucket;
    iter.curr.i = RNIL;
    return false;
  }

  iter.bucket = bucket;
  iter.curr.i = hashValues[bucket];
  iter.curr.p = thePool.getPtr(iter.curr.i);
  return true;
}

template <typename P, typename T, typename M>
inline
bool
DLMHashTable<P, T, M>::seize(Ptr<T> & ptr)
{
  if (thePool.seize(ptr)){
    M::nextHash(*ptr.p) = M::prevHash(*ptr.p) = RNIL;
    return true;
  }
  return false;
}

template <typename P, typename T, typename M>
inline
void
DLMHashTable<P, T, M>::getPtr(Ptr<T> & ptr, Uint32 i) const
{
  ptr.i = i;
  ptr.p = thePool.getPtr(i);
}

template <typename P, typename T, typename M>
inline
void
DLMHashTable<P, T, M>::getPtr(Ptr<T> & ptr) const
{
  thePool.getPtr(ptr);
}

template <typename P, typename T, typename M>
inline
T *
DLMHashTable<P, T, M>::getPtr(Uint32 i) const
{
  return thePool.getPtr(i);
}

template <typename P, typename T, typename M>
inline
bool
DLMHashTable<P, T, M>::find(Ptr<T> & ptr, const T & key) const
{
  const Uint32 hv = M::hashValue(key) & mask;

  Uint32 i;
  T * p;

  i = hashValues[hv];
  while (i != RNIL)
  {
    p = thePool.getPtr(i);
    if (M::equal(key, * p))
    {
      ptr.i = i;
      ptr.p = p;
      return true;
    }
    i = M::nextHash(*p);
  }
  ptr.i = RNIL;
  ptr.p = NULL;
  return false;
}

// Specializations

template <typename P, typename T, typename U = T >
class DLHashTableImpl: public DLMHashTable<P, T, DLHashTableDefaultMethods<T, U> >
{
public:
  explicit DLHashTableImpl(P & p): DLMHashTable<P, T, DLHashTableDefaultMethods<T, U> >(p) { }
private:
  DLHashTableImpl(const DLHashTableImpl&);
  DLHashTableImpl&  operator=(const DLHashTableImpl&);
};

template <typename T, typename U = T, typename P = ArrayPool<T> >
class DLHashTable: public DLMHashTable<P, T, DLHashTableDefaultMethods<T, U> >
{
public:
  explicit DLHashTable(P & p): DLMHashTable<P, T, DLHashTableDefaultMethods<T, U> >(p) { }
private:
  DLHashTable(const DLHashTable&);
  DLHashTable&  operator=(const DLHashTable&);
};


#undef JAM_FILE_ID

#endif
