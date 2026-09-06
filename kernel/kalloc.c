// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

// Reference counts for physical pages, so that a page shared between
// several process page tables (as in copy-on-write fork) is freed only
// when the last reference to it disappears.  Indexed by physical page
// number relative to KERNBASE.
#define PA2REFIDX(pa) (((uint64)(pa) - KERNBASE) / PGSIZE)
#define NREF ((PHYSTOP - KERNBASE) / PGSIZE)

struct {
  struct spinlock lock;
  int count[NREF];
} ref;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref.lock, "ref");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Record one more process page table mapping the physical page at pa.
void
incref(uint64 pa)
{
  acquire(&ref.lock);
  ref.count[PA2REFIDX(pa)]++;
  release(&ref.lock);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
// A page that is mapped by more than one page table is not really
// freed: this call just drops one reference to it.
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&ref.lock);
  if(ref.count[PA2REFIDX(pa)] > 1){
    // Still referenced by other page tables: drop one reference only.
    ref.count[PA2REFIDX(pa)]--;
    release(&ref.lock);
    return;
  }
  ref.count[PA2REFIDX(pa)] = 0;
  release(&ref.lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    acquire(&ref.lock);
    ref.count[PA2REFIDX(r)] = 1;
    release(&ref.lock);
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  return (void*)r;
}
