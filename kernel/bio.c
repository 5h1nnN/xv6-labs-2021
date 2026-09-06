// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.
//
// The cache is organized as a hash table of buckets, each with its own
// lock, so lookups/releases of different blocks usually proceed in
// parallel.  A single global lock is only taken when a block is not in
// the cache and an LRU victim has to be chosen (eviction).


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

struct bucket {
  struct spinlock lock;
  struct buf head;  // doubly-linked list of buffers in this bucket
};

struct {
  struct spinlock lock;     // serializes eviction / allocation of buffers
  struct buf buf[NBUF];
  struct bucket buckets[NBUCKET];
} bcache;

static uint
bh(uint blockno)
{
  return blockno % NBUCKET;
}

void
binit(void)
{
  struct buf *b;
  struct bucket *bk;

  initlock(&bcache.lock, "bcache");

  // Initialize each bucket's lock and empty list.
  for(int i = 0; i < NBUCKET; i++){
    bk = &bcache.buckets[i];
    initlock(&bk->lock, "bcache.bucket");
    bk->head.prev = &bk->head;
    bk->head.next = &bk->head;
  }

  // Put every buffer on the list of bucket 0 (blockno 0 hashes there).
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
    b->refcnt = 0;
    b->ticks = 0;
    bk = &bcache.buckets[0];
    b->next = bk->head.next;
    b->prev = &bk->head;
    bk->head.next->prev = b;
    bk->head.next = b;
  }
}

// Look through the buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  struct bucket *bk = &bcache.buckets[bh(blockno)];

  // Is the block already cached?  Only the target bucket needs to be
  // searched, since a buffer's bucket never changes while it is
  // referenced (its block number changes only during eviction, when
  // the buffer is unreferenced).
  acquire(&bk->lock);
  for(b = bk->head.next; b != &bk->head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bk->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bk->lock);

  // Not cached.  Eviction is serialized by the global lock.
  acquire(&bcache.lock);

  // Re-check: another CPU may have inserted the block between our
  // first search and acquiring the global lock.
  acquire(&bk->lock);
  for(b = bk->head.next; b != &bk->head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bk->lock);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bk->lock);

  // Choose the least-recently-used unreferenced buffer and move it
  // into the target bucket.
  for(int tries = 0; ; tries++){
    struct buf *victim = 0;
    uint oldest = 0xffffffff;

    for(int i = 0; i < NBUCKET; i++){
      struct bucket *o = &bcache.buckets[i];
      acquire(&o->lock);
      for(b = o->head.next; b != &o->head; b = b->next){
        if(b->refcnt == 0 && b->ticks <= oldest){
          oldest = b->ticks;
          victim = b;
        }
      }
      release(&o->lock);
    }
    if(victim == 0)
      panic("bget: no buffers");

    // Detach the victim from its bucket, re-checking that it is
    // still unreferenced (a concurrent bget could have taken it).
    struct bucket *old = &bcache.buckets[bh(victim->blockno)];
    acquire(&old->lock);
    if(victim->refcnt != 0){
      release(&old->lock);
      if(tries > 1000)
        panic("bget: livelock");
      continue;
    }
    victim->prev->next = victim->next;
    victim->next->prev = victim->prev;

    if(old != bk)
      acquire(&bk->lock);
    victim->dev = dev;
    victim->blockno = blockno;
    victim->valid = 0;
    victim->disk = 0;
    victim->refcnt = 1;
    victim->ticks = 0;
    victim->next = bk->head.next;
    victim->prev = &bk->head;
    bk->head.next->prev = victim;
    bk->head.next = victim;
    if(old != bk)
      release(&bk->lock);
    release(&old->lock);
    release(&bcache.lock);
    acquiresleep(&victim->lock);
    return victim;
  }
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.  Record the time of its last use so that
// bget() can pick an LRU victim.  No global lock is needed: refcnt and
// ticks are protected by the buffer's bucket lock, and while the buffer
// is referenced it can't be evicted or moved to another bucket.
void
brelse(struct buf *b)
{
  struct bucket *bk;

  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  bk = &bcache.buckets[bh(b->blockno)];
  acquire(&bk->lock);
  b->refcnt--;
  if(b->refcnt == 0){
    b->ticks = ticks;   // LRU timestamp (extern in defs.h)
  }
  release(&bk->lock);
}

void
bpin(struct buf *b) {
  struct bucket *bk = &bcache.buckets[bh(b->blockno)];
  acquire(&bk->lock);
  b->refcnt++;
  release(&bk->lock);
}

void
bunpin(struct buf *b) {
  struct bucket *bk = &bcache.buckets[bh(b->blockno)];
  acquire(&bk->lock);
  b->refcnt--;
  release(&bk->lock);
}
