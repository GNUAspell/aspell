#ifndef ACOMMON_CACHE__HPP
#define ACOMMON_CACHE__HPP

#include "lock.hpp"
#include "posib_err.hpp"

namespace aspell {

class Cacheable;

class GlobalCacheBase
{
public:
  mutable Mutex lock;
public: // but don't use
  const char * name;
  GlobalCacheBase * next;
  GlobalCacheBase * * prev;
protected:
  Cacheable * first;
  void del(Cacheable * d);
  void add(Cacheable * n);
  GlobalCacheBase(const char * n);
  ~GlobalCacheBase();
public:
  void release(Cacheable * d);
  void detach(Cacheable * d);
  void detach_all();
};

template <class D>
class GlobalCache : public GlobalCacheBase
{
public:
  typedef D Data;
  typedef typename Data::CacheKey Key;
public:
  GlobalCache(const char * n) : GlobalCacheBase(n) {}
  // "find" and "add" will _not_ acquire a lock
  Data * find(const Key & key) {
    D * cur = static_cast<D *>(first);
    while (cur && !cur->cache_key_eq(key))
      cur = static_cast<D *>(cur->next);
    return cur;
  }
  void add(Data * n) {GlobalCacheBase::add(n);}
  // "release" and "detach" _will_ acquire a lock
  void release(Data * d) {GlobalCacheBase::release(d);}
  void detach(Data * d) {GlobalCacheBase::detach(d);}
};

// get_cache_data (both versions) and release_cache_data will acquires
// the cache's lock

template <class Data>
PosibErr<Data *> get_cache_data(GlobalCache<Data> * cache, 
                                const typename Data::CacheConfig * config, 
                                const typename Data::CacheKey & key)
{
  LOCK(&cache->lock);
  Data * n = cache->find(key);
  //CERR << "Getting " << key << " for " << cache->name << "\n";
  if (n) {
    n->refcount++;
    return n;
  }
  PosibErr<Data *> res = Data::get_new(key, config);
  if (res.has_err()) {
    //CERR << "ERROR\n"; 
    return res;
  }
  n = res.data;
  cache->add(n);
  //CERR << "LOADED FROM DISK\n";
  return n;
}

template <class Data>
PosibErr<Data *> get_cache_data(GlobalCache<Data> * cache, 
                                const typename Data::CacheConfig * config, 
                                const typename Data::CacheConfig2 * config2,
                                const typename Data::CacheKey & key)
{
  LOCK(&cache->lock);
  Data * n = cache->find(key);
  //CERR << "Getting " << key << "\n";
  if (n) {
    n->refcount++;
    return n;
  }
  PosibErr<Data *> res = Data::get_new(key, config, config2);
  if (res.has_err()) {
    //CERR << "ERROR\n"; 
    return res;
  }
  n = res.data;
  cache->add(n);
  //CERR << "LOADED FROM DISK\n";
  return n;
}

void release_cache_data(GlobalCacheBase *, const Cacheable *);
static inline void release_cache_data(const GlobalCacheBase * c, const Cacheable * d)
{
  release_cache_data(const_cast<GlobalCacheBase *>(c),d);
}

class Cacheable
{
public: // but don't use
  Cacheable * next;
  Cacheable * * prev;
  mutable int refcount;
  GlobalCacheBase * cache;
public:
  bool attached() {return prev;}
  void copy_no_lock() const {refcount++;}
  void copy() const; // Acquires cache->lock
  void release() const {release_cache_data(cache,this);} // Acquires cache->lock
  Cacheable(GlobalCacheBase * c = 0) : next(0), prev(0), refcount(1), cache(c) {}
  virtual ~Cacheable() {}
};

template <class Data>
class CachePtr
{
  Data * ptr;

public:
  void reset(Data * p) {
    if (ptr) ptr->release();
    ptr = p;
  }
  void copy(Data * p) {if (p) p->copy(); reset(p);}
  Data * release() {Data * tmp = ptr; ptr = 0; return tmp;}

  Data & operator*  () const {return *ptr;}
  Data * operator-> () const {return ptr;}
  Data * get()         const {return ptr;}
  operator Data * ()   const {return ptr;}

  CachePtr() : ptr(0) {}
  CachePtr(const CachePtr & other) {ptr = other.ptr; if (ptr) ptr->copy();}
  void operator=(const CachePtr & other) {copy(other.ptr);}
  ~CachePtr() {reset(0);}
};

template <class Data>
PosibErr<void> setup(CachePtr<Data> & res,
                     GlobalCache<Data> * cache, 
                     const typename Data::CacheConfig * config, 
                     const typename Data::CacheKey & key) {
  PosibErr<Data *> pe = get_cache_data(cache, config, key);
  if (pe.has_err()) return pe;
  res.reset(pe.data);
  return no_err;
}

bool reset_cache(const char * = 0);

}

#endif
