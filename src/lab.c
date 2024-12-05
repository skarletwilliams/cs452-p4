#include <stdlib.h>
#ifdef __APPLE__
#include <sys/errno.h>
#else
#include <errno.h>
#endif
#include "../src/lab.h"
#include <sys/mman.h>

//TODO: make a function that takes a void * pointer and a length and sets values to zero
void buddy_wipe(struct buddy_pool *pool, struct avail * block) {
  //TODO: WIPE
}

//TODO: Define in header
void add_to_avail(struct buddy_pool *pool, struct avail * block, size_t k) {
  if(block->kval != k) {exit(2);} //Should not be requesting to add a block of incorrect size to wrong list

  printf("Adding to avail[%ld]!\n\tList.OG.next: %ld\n\tList.OG.prev: %ld\n\tBlock address: %ld\n", k, pool->avail[k].next, pool->avail[k].prev, (size_t) block);
  block->prev = pool->avail[k].prev;
  block->next = pool->avail[k].next;
  printf("Ensure new block matches!\n\tblock.next: %ld\n\tblock.prev: %ld\n", block->next, block->prev);
  pool->avail[k].prev->next = block;
  pool->avail[k].next->prev = block;
  pool->avail[k].next = block;
    printf("New list should point to new block now!!\n\tList.OG.next.prev: %ld\n\tList.OG.prev.next: %ld\n\tAvail[%ld].next: %ld\n", pool->avail[k].next->prev, pool->avail[k].prev->next, k, pool->avail[k].next);
}

//TODO: Define in header
void buddy_merge(struct buddy_pool *pool, struct avail *buddy1, struct avail *buddy2) {
  size_t k = buddy1->kval;
  if (k != buddy2->kval) { exit(1); } //Shouldn't be requesting to merge buddies that are not buddies!!
  
  //Remove buddy 1 from list
  buddy1->prev->next = buddy1->next;
  buddy1->next->prev = buddy1->prev;

  //Remove buddy 2 from list
  buddy2->prev->next = buddy2->next;
  buddy2->next->prev = buddy2->prev;
  
  //Unused block in avail[k] list
  pool->avail[k].next = &pool->avail[k];
  pool->avail[k].prev = &pool->avail[k];
  pool->avail[k].kval = k;
  pool->avail[k].tag = BLOCK_UNUSED;

  //Available block in avail[k+1] list
  size_t ptr1 = (size_t)buddy1;
  size_t ptr2 = (size_t)buddy2;
  ptr1 &= ~(UINT64_C(1) << k);
  ptr2 &= ~(UINT64_C(1) << k);
  if (ptr1 != ptr2) { exit(1); } //These should be the same address!

  k++;
  struct avail * parent = (struct avail *) ptr1;
  parent->kval = k;
  parent->tag = BLOCK_AVAIL;
  buddy_wipe(pool, parent);
  add_to_avail(pool, parent, k);

  if(k < pool->kval_m) {
    printf("Searching for ANOTHER buddy...\n");
    struct avail * buddy = buddy_calc(pool, parent);
    if(buddy != NULL) {printf("\tBuddy found!\n");buddy_merge(pool, parent, buddy);}
  }
}

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
  size_t k = buddy->kval;

  //Check available list to see if the buddy is available.
  printf("\tI am located at\n\t\t: %ld\n\List %ld contains:\n", (size_t)buddy, k);
  struct avail * friend = pool->avail[k].next;
  printf("\t\t: %ld\n", (size_t)friend);
  printf("\t\t: %ld\n", (size_t)friend->next);
  printf("\t\t: %ld\n", (size_t)friend->next->next);

  if(friend == buddy && friend->next == buddy) { return NULL; }
    
  return friend;
}

void *buddy_malloc(struct buddy_pool *pool, size_t size) {
  if (pool == NULL || size == 0) { return NULL;}
  // First we need to know what size k we are looking for.
  size += sizeof(struct avail);
  size_t k = btok(size);
  //Fit constraints - you can't accomodate someone asking for larger than max k
  if(k < SMALLEST_K) {k = SMALLEST_K;}
  void *ptr = NULL;

  // Find a block that can accomodate this in avail[k] or above
  for (long unsigned int i = k; i <= pool->kval_m; i++) {
    struct avail *next = (struct avail *) pool->avail[i].next;
    while(next->tag != BLOCK_UNUSED) { //If block is unused, end of list!
      if (next->tag == BLOCK_AVAIL) { ptr = next; break; } //Found available block, exit while loop 
      next = next->next; // Else, next in list
    }
    if (ptr != NULL) {break;} //If block is found, stop searching!
  }
  
  // No block was found, terminate
  if (ptr == NULL) { return NULL;}

  // Reserve Block
  struct avail *block = (struct avail *) ptr;
  block->prev->next = block->next;
  block->next->prev = block->prev;
  block->tag = BLOCK_RESERVED;

  while (block->kval != k) {
    //Divide buddies
    block->kval--;

    // Manage XX100 buddy to add to list
    void * ptr_r = ptr;
    ptr_r += UINT64_C(1) << (block->kval); 
    printf("Splitting in two!--------------------------------------\n");
    printf("\tI am located at:\n\t\t%ld\n\tMy budddy is located at:\n\t\t%ld\n", ptr, ptr_r);
    struct avail * block_r = (struct avail *) ptr_r;
    block_r->tag = BLOCK_AVAIL;
    block_r->kval = block->kval;
    add_to_avail(pool, block_r, block_r->kval);
    printf("Buddy put in list %ld.--------------------------------------\n", block_r->kval);
  }
  
  return ptr + sizeof(struct avail); 
}

void buddy_free(struct buddy_pool *pool, void *ptr) {
  if (ptr == NULL) {return;}
  ptr -= sizeof(struct avail); // Now points to the block
  struct avail *block = (struct avail *) ptr;
  size_t k = block->kval;
  block->tag = BLOCK_AVAIL;

  //Wipe buddy
  buddy_wipe(pool, block);
    
  //Simply add the block after the head
  add_to_avail(pool, block, k);

  //If k < pool.kval_m, and buddy is available, merge
  if(k < pool->kval_m) {
    printf("Searching for buddy...\n");
    struct avail * buddy = buddy_calc(pool, block);
    if(buddy != NULL) {printf("\tBuddy found!\n");buddy_merge(pool, block, buddy);}
  }
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
