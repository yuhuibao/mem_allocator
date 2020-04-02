/*
#include "types.h"
#include "stat.h"
#include "user.h"
#include "param.h"
*/

#include "xmalloc.h"
#include <pthread.h>
#include <string.h>
#include <sys/mman.h>
// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.
//
// Then copied from xv6.

// TODO: Remove this stuff
// TODO: end of stuff to remove


typedef long Align;

union header {
  struct {
    union header *ptr;
    size_t size;
  } s;
  Align x;
};

typedef union header Header;

// TODO: This is shared global data.
// You're going to want a mutex to protect this.
static Header base;
static Header *freep;
pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;

void
xfree_helper(void *ap)
{
  Header *bp, *p;

  bp = (Header*)ap - 1;
  for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;
  if(bp + bp->s.size == p->s.ptr){
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  } else
    bp->s.ptr = p->s.ptr;
  if(p + p->s.size == bp){
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  } else
    p->s.ptr = bp;
  freep = p;
}
void
xfree(void *ap)
{
  pthread_mutex_lock(&lock);
  
  xfree_helper(ap);

  pthread_mutex_unlock(&lock);
}
static Header*
morecore(size_t nu)
{
  char *p;
  Header *hp;

  if(nu < 4096)
    nu = 4096;
  // TODO: Replace sbrk use with mmap
  p = mmap(0,nu * sizeof(Header),PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  if(p == (char*)-1)
    return 0;
  hp = (Header*)p;
  hp->s.size = nu;
  xfree_helper((void*)(hp + 1));
  return freep;
}

void*
xmalloc(size_t nbytes)
{
  Header *p, *prevp;
  size_t nunits;
  pthread_mutex_lock(&lock);

  nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;
  if((prevp = freep) == 0){
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }
  for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
    if(p->s.size >= nunits){
      if(p->s.size == nunits)
        prevp->s.ptr = p->s.ptr;
      else {
        p->s.size -= nunits;
        p += p->s.size;
        p->s.size = nunits;
      }
      freep = prevp;
      pthread_mutex_unlock(&lock);
      return (void*)(p + 1);
    }
    if(p == freep){
      
      p=morecore(nunits);
      
      if(p == 0){
        pthread_mutex_unlock(&lock);
        return 0;
      }
    }
  }
}

void*
xrealloc(void* prev, size_t nn)
{
  // TODO: Actually build realloc.
  void* newptr = xmalloc(nn);
  size_t size = ((Header*)prev-1)->s.size * sizeof(Header) - sizeof(Header);
  memcpy((Header*)newptr,(Header*)prev,size);
  xfree(prev);
  return newptr;
}
