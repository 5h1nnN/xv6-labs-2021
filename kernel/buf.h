struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  uint ticks;    // last use time, for LRU eviction
  struct buf *prev; // bucket list
  struct buf *next;
  uchar data[BSIZE];
};

