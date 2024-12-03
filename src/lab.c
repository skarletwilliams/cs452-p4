#include <stdlib.h>
#ifdef __APPLE__
#include <sys/errno.h>
#else
#include <errno.h>
#endif
#include "../src/lab.h"
#include <sys/mman.h>

size_t btok(size_t bytes) {
  unsigned int count = 0; // For an accurate btok, start at 1. bit shifting in tests is innaccurate
  bytes -= 1;
  while (bytes > 0) {
    bytes >>= 1;
    count++;
  }
  return count;
}

struct avail *buddy_calc(struct buddy_pool *pool, struct avail *buddy) {

  return 0;
}

void *buddy_malloc(struct buddy_pool *pool, size_t size) {
  if (pool == NULL || size == 0) { return NULL;}
  // First we need to know what size k we are looking for.
  size += sizeof(struct avail);
  size_t k = btok(size);
  //Fit constraints - you can't accomodate someone asking for larger than max k
  if(k < SMALLEST_K) {k = SMALLEST_K;}
  struct avail *availBlock = NULL;

  // Find a block that can accomodate this in avail[k] or above
  for (int i = k; i <= pool->kval_m; i++) {
    struct avail *next = (struct avail *) pool->avail[i].next; // Skip first block, as it is unusable head.
    while(next->tag != BLOCK_UNUSED) { //If next is block unused, end of list!
      if (next->tag == BLOCK_AVAIL) { availBlock = (struct avail *) next; break; } //Found available block, exit while loop 
      next = (struct avail *) pool->avail[i].next; // Else, next in list
    }
    if (availBlock != NULL) {break;} //If block is found, stop searching!
  }
  
  // No block was found, terminate
  if (availBlock == NULL) { return NULL;}

  // Remove from list
  availBlock->prev->next = availBlock->next;
  availBlock->next->prev = availBlock->prev;
  availBlock->tag = BLOCK_RESERVED;

  // If accurate size, terminate and return memory block
  if (availBlock->kval == k) { return (availBlock + 1);}//Note1: why + 1??? why not + sizeof(struct avail));}

  // otherwise split block until requested size
  for (int i = availBlock->kval; i >= k; i--) {

  }

  return availBlock;
}

void buddy_free(struct buddy_pool *pool, void *ptr) {
  if (ptr == NULL) {return NULL;}
  struct avail *block = (struct avail *) ptr - 1; // NOTE1: - 1 like in the test?
  size_t k = block->kval;
  block->tag = BLOCK_AVAIL;
    
  //Simply add the block after the head
  block->prev = &pool->avail[k];
  block->next = pool->avail[k].next;
  pool->avail[k].next->prev = block;
  pool->avail[k].next = block;
}

void *buddy_realloc(struct buddy_pool *pool, void *ptr, size_t size)  {
  
  return 0;
}

void buddy_init(struct buddy_pool *pool, size_t size) {
  if (size == 0) {size = UINT64_C(1) << DEFAULT_K;} // Default size if no size set
  else if (size > (UINT64_C(1) << MAX_K)) {size = (UINT64_C(1) << MAX_K);} // Max size if size too large
  else if (size < (UINT64_C(1) << MIN_K)) {size = (UINT64_C(1) << MIN_K);} // Min size if size too small

  pool->kval_m = btok(size);
  pool->numbytes = UINT64_C(1) << pool->kval_m;

  pool->base = mmap(NULL, pool->numbytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (pool->base == MAP_FAILED) {
    perror("buddy: could not allocate memory pool!");
  }

  for (unsigned int i = 0; i < pool->kval_m; i++) {
    pool->avail[i].next = &pool->avail[i];
    pool->avail[i].prev = &pool->avail[i];
    pool->avail[i].kval = i;
    pool->avail[i].tag = BLOCK_UNUSED;
  }

  pool->avail[pool->kval_m].next = pool->base;
  pool->avail[pool->kval_m].prev = pool->base;
  struct avail *ptr = (struct avail *) pool->base;
  ptr->tag = BLOCK_AVAIL;
  ptr->kval = pool->kval_m;
  ptr->next = &pool->avail[pool->kval_m];
  ptr->prev = &pool->avail[pool->kval_m];
}

void buddy_destroy(struct buddy_pool *pool) {
  int status = munmap(pool->base, pool->numbytes);
	if (status == -1) {
		perror("buddy: destroy failed!");
	}
}

