// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
  
  int refer[PHYSTOP / PGSIZE];
  int freepage;  

} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE){

    kmem.refer[V2P(p)/PGSIZE] = 0;

    kfree(p);

  }
}
//PAGEBREAK: 21
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  //memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = (struct run*)v;
  
  if(kmem.refer[V2P(v)/PGSIZE] > 0) 
    kmem.refer[V2P(v)/PGSIZE] = kmem.refer[V2P(v)/PGSIZE] - 1;
  /*
  r->next = kmem.freelist;
  kmem.freelist = r;
  */

  if(kmem.refer[V2P(v)/PGSIZE] == 0){   
    
    memset(v, 1, PGSIZE); 
    r->next = kmem.freelist;
    kmem.freepage = kmem.freepage + 1;
    kmem.freelist = r;
  }

  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;

    kmem.refer[V2P((char*)r) / PGSIZE] = 1;
    kmem.freepage--;
  
  }

  if(kmem.use_lock)
    release(&kmem.lock);

  return (char*)r;
}

void 
incr_refc(uint pa)
{
  acquire(&kmem.lock);
  
  int index = pa / PGSIZE;
  kmem.refer[index]++;

  release(&kmem.lock);

}

void 
decr_refc(uint pa)
{
  acquire(&kmem.lock);
  
  int index = pa / PGSIZE;
  kmem.refer[index]--;
  
  release(&kmem.lock);
}

int 
get_refc(uint pa)
{
  int tmp;

  acquire(&kmem.lock);

  int index = pa / PGSIZE;
  tmp = kmem.refer[index];
  
  release(&kmem.lock);

  return tmp;

}

int 
countfp(void)
{
  acquire(&kmem.lock);

  uint tmp = kmem.freepage;
  
  release(&kmem.lock);
 
  return tmp;
} 
