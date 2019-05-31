/*
 * Name: Robert Vitali
 * Email: robert.vitali@wustl.edu
 * This is a functioning explicit free list memory allocator program
 * An explicit list allocator allocates and frees memory. When memory is
 * free'd from the list of memory, it is added to a free list. This allows for
 * quick allocating and freeing to the list of memory. When there is no free
 * memory, this implementation works like a implicit list allocator in that
 * it extends the list of memory so that more blocks can be added. When an item
 * is being allocated, the free list is traversed so that the first free block
 * that can fit the required size is chosen. This block is either a perfect fit
 * or it is too big. When it is too big, the extra space not needed is put back
 * to the free list. To avoid extremely small pockets of free space, coalescing
 * occurs to check to the left and right of a block to see if it can combine to
 * become a bigger block. With this implementation, the new free blocks are
 * added to the beginning of the free list.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"

/*MACROS*/
#define ALIGNMENT 8
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((void *)(bp) - DSIZE))

//next pointer and previous pointer in free list
#define GET_NEXT_FP(p) (*(char **)(p + WSIZE))
#define GET_PREVIOUS_FP(p) (*(char **)(p))

/*setting next and previous pointers in free list*/
#define SET_NEXT_FP(p1, p2) (GET_NEXT_FP(p1) = p2)
#define SET_PREVIOUS_FP(p1, p2) (GET_PREVIOUS_FP(p1) = p2)

/*global variables*/
static void *heap_listp; /*entire heap*/
static void *free_listp; /*free list*/

/*functions*/
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void add_free_block(char *bp);
static void delete_free_block(char *bp);
static void add_to_heap(char *p1, char *p2, size_t size, int x);
//static void mm_check(); This is commented out to avoid warnings

/*
 * mm_init - initialize the malloc package.
 * this creates a heap with an initial free block
 */
int mm_init(void){

  if((heap_listp = mem_sbrk(8*WSIZE)) == NULL){
    return -1;
  }
  PUT(heap_listp, 0);
  PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
  PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
  PUT(heap_listp + (3*WSIZE), PACK(0, 1));

  heap_listp = heap_listp + DSIZE + WSIZE;
  free_listp = heap_listp;

  if(extend_heap(CHUNKSIZE/WSIZE) == NULL){
    return -1;
  }
  return 0;
}

/*
 * extends the heap with a new free block
 * size_t words used for alignment
 */
static void *extend_heap(size_t words){
  char *bp;
  size_t size;

  size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
  if((long)(bp = mem_sbrk(size)) == -1){
    return NULL;
  }

  add_to_heap(bp, bp, size, 0);
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));
  return coalesce(bp);
}

/*
 * mm_free - frees a block and coalesces to merge with any adjacent blocks
 * void *bp is the block pointer of what we want to free
 */
void mm_free(void *bp){
  size_t size = GET_SIZE(HDRP(bp));
  add_to_heap(bp, bp, size, 0);
  coalesce(bp);
}

/*
 * coalesce - checks adjacent blocks using previous and next allocate
 * depending on case will merge bp with previous, next or both
 * void *bp is the block pointer we are checking
 */
static void *coalesce(void *bp){
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp))) || PREV_BLKP(bp) == bp;
  size_t size = GET_SIZE(HDRP(bp));

  if(prev_alloc && next_alloc){
    add_free_block(bp);
    return bp;
  }

  else if(prev_alloc && !next_alloc){
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    delete_free_block(NEXT_BLKP(bp));
    add_to_heap(bp, bp, size, 0);
    add_free_block(bp);
  }

  else if(!prev_alloc && next_alloc){
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    add_to_heap(PREV_BLKP(bp), bp, size, 0);
    bp = PREV_BLKP(bp);
    delete_free_block(bp);
    add_free_block(bp);
  }

  else{
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    add_to_heap(PREV_BLKP(bp), NEXT_BLKP(bp), size, 0);
    delete_free_block(PREV_BLKP(bp));
    delete_free_block(NEXT_BLKP(bp));
    bp = PREV_BLKP(bp);
    add_free_block(bp);
  }
  return bp;
}

/*
 * mm_malloc - allocates a block from the free list
 * size_t size is used for block size search and adjustment
 */
void *mm_malloc(size_t size){
  size_t asize;
  size_t extendsize;
  char *bp;

  if(size == 0){
    return NULL;
  }

  if(size <= DSIZE){
    asize = 2*DSIZE;
  }else{
    asize = DSIZE * (((size + DSIZE + DSIZE-1))/DSIZE);
  }

  if((bp = find_fit(asize)) != NULL){
    place(bp, asize);
    return bp;
  }

  extendsize = MAX(asize, CHUNKSIZE);
  if((bp = extend_heap(extendsize/WSIZE)) == NULL){
    return NULL;
  }

  place(bp, asize);
  return bp;
}

/*
 * find fit is used to find a free block
 * size_t asize is specified block size that is being searched for
 * this search is a first fit search
 */
static void *find_fit(size_t asize){
  void *bp;

    for(bp = free_listp; bp != NULL; bp = GET_NEXT_FP(bp)){

      if(asize <= GET_SIZE(HDRP(bp))){

        return bp;

      }
    }
  return NULL;

}

/*
 * place - places a block onto the heap_listp
 * void *bp is used to specify the block being placed
 * size_t asize is used to determine if we have extra space
 * in the block
 */
static void place(void *bp, size_t asize){
  size_t csize = GET_SIZE(HDRP(bp));
  size_t difference = csize - asize;

  if(difference >= (2*DSIZE)){
    delete_free_block(bp);
    add_to_heap(bp, bp, asize, 1);
    bp = NEXT_BLKP(bp);
    add_to_heap(bp, bp, difference, 0);
    coalesce(bp);
  }
  else{
    delete_free_block(bp);
    add_to_heap(bp, bp, csize, 1);
  }
}

/*
 * mm_realloc - returns a pointer to an allocated region
 * void *ptr is used to specify the location we want to check for realloc
 * size_t size is used to specify the minimum size
 */
void *mm_realloc(void *ptr, size_t size){
    void *temp = NULL;

    if(ptr == NULL) mm_malloc(size);

    else if(size == 0) mm_free(ptr);

    else{
    temp = mm_malloc(size);
    size = size < GET_SIZE(HDRP(ptr)) ? size : GET_SIZE(HDRP(ptr));
    memcpy(temp, ptr, size);
    mm_free(ptr);
    }

    return temp;
}

/*
 * add_free_blcok - add free blocks to the beginning of the list
 * char *bp is used to give the free block being added
 */
static void add_free_block(char *bp){
  if(free_listp == NULL){
    free_listp = bp;
    return;
  }

  SET_NEXT_FP(bp,free_listp);
  SET_PREVIOUS_FP(free_listp,bp);
  SET_PREVIOUS_FP(bp,NULL);
  free_listp = bp;
 }

/*
 * delete_free_block - remove the items from the free list
 * char *bp is used to specify the block set for deletion
 */
static void delete_free_block(char *bp){
  if(bp == free_listp){

    if(GET_NEXT_FP(bp) == NULL){
      free_listp = NULL;
      return;
    }

    free_listp = GET_NEXT_FP(bp);
    SET_PREVIOUS_FP(GET_NEXT_FP(bp), GET_PREVIOUS_FP(bp));
  }
  else if(GET_NEXT_FP(bp) == NULL) SET_PREVIOUS_FP(bp, NULL);

  else{
    SET_PREVIOUS_FP(GET_NEXT_FP(bp), GET_PREVIOUS_FP(bp));
    SET_NEXT_FP(GET_PREVIOUS_FP(bp), GET_NEXT_FP(bp));
  }
}


/*
 * add_to_heap - make code cleaner from put statements and avoid syntax errors
 * char *p1 specifies the first pointer
 * char *p2 specifies the second pointer (if it is different)
 * size_t size specifies the size to pack into a word
 * int x specifies the allocated bit to pack into a word
 */
static void add_to_heap(char *p1, char *p2,  size_t size, int x){
  PUT(HDRP(p1), PACK(size, x));
  PUT(FTRP(p2), PACK(size, x));
}



/*
 * mm_check - check the previous current and next locations
 * also prints out header, footer and size to make sure no inconsistencies
 */
 /*
static void mm_check(){
  void *bp;
  if(free_listp != NULL){
    for(bp = free_listp; bp != NULL; bp = GET_NEXT_FP(bp)){
      fprintf(stderr, "\nMM_CHECK: \nPREVIOUS:%p", GET_PREVIOUS_FP(bp));
      fprintf(stderr, "\nMM_CHECK: \nHEADER:%p", HDRP(bp));
      fprintf(stderr, "\nMM_CHECK: \nSIZE HEADER:%d", GET_SIZE(HDRP(bp)));
      fprintf(stderr, "\nMM_CHECK: \nCURRENT:%p", bp);
      fprintf(stderr, "\nMM_CHECK: \nFOOTER:%p", FTRP(bp));
      fprintf(stderr, "\nMM_CHECK: \nSIZE FOOTER:%d", GET_SIZE(FTRP(bp)));
      fprintf(stderr, "\nMM_CHECK: \nNEXT:%p", GET_NEXT_FP(bp));
    }
  }
}*/
