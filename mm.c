/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "6주차-9조",
    /* First member's full name */
    "김세연",
    /* First member's email address */
    "syk2245@gmail.com",
    /* Second member's full name (leave blank if none) */
    "안우현",
    /* Second member's email address (leave blank if none) */
    "김회일"};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

//////////////////////////묵시적 가용리스트 만들기//////////////////////////////////////////////
#define WSIZE 4             // 워드, 헤더, 푸터 크기(바이트)
#define DSIZE 8             // 더블워드 크기(바이트)
#define CHUNKSIZE (1 << 12) // 힙의 증가량(바이트)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc)) // size와 allocated bit을 워드로 포장

// 주소 p에서 워드를 읽고 쓰기
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

// 주소 p에서 크기와 할당된 필드 읽기
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// 블럭 포인터 bp가 주어졌을 때 header, footer의 주소값 계산하기
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// 블럭 포인터 bp가 주어졌을 때 이전과 이후 블럭의 주소 계산하기
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

//////////////////////////전역변수 선언//////////////////////////////////////////////////////
static char *heap_listp;

/////////////////////////프로토타입 선언/////////////////////////////////////////////////////
int mm_init(void);
static void *extend_heap(size_t words);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *mm_realloc(void *ptr, size_t size);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/////////////////////////////////////////////////////////////////////////////////////////
/*
 * mm_init - initialize the malloc package.
 * 1. 메모리 시스템으로부터 4개의 워드 할당받기
 * 2. 빈 가용 리스트(free list)를 만들기 위해 초기화
 * 3. extend_heap 호출 -> CHUNCKSIZE 바이트만큼 힙을 증가시키고 초기 가용 블럭 창조
 */
int mm_init(void)
{
   // 메모리
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
    {
        return -1;
    }
    // 1. 빈 가용 리스트를 만들도록 초기화
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // 프롤로그 헤더
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // 프롤로그 푸터
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1)); // 에필로그 헤더
    heap_listp += (2 * WSIZE);

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    {
        return -1;
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
/*
 * extend_heap function - initialize the malloc package.
 * 1. 요청 된 크기를 가장 가까운 크기의 더블워드의 배수로 올림
 * 2. 메모리 시스템으로부터 크기를 요청
 * 3. 기존 블럭이 가용할 수 있다면 합치기
 */
static void *extend_heap(size_t words)
{
    char *bp; // 블럭포인터
    size_t size; // 할당받을 공간의 크기

    // 정렬조건을 유지하기 위한 로직
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
    {
        return NULL;
    }

    // 가용 블럭 header, footer와 epilogue 헤더를 초기화함 
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    // 이전 블럭이 해제되었다면 합치기
    return coalesce(bp);
}

/////////////////////////////////////////////////////////////////////////////////////////

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0)
    {
        return NULL;
    }

    if (size <= DSIZE)
    {
        asize = 2 * DSIZE;
    }
    else
    {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
    {
        return NULL;
    }
    place(bp, asize);
    return bp;
}

//////////////////////////////////////////////////////////////////////////////////////////
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{// TODO: 이건 해제 시 별도로 아무런 행동을 하지 않아도 되나?
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    coalesce(bp);
}
//////////////////////////////////////////////////////////////////////////////////////////
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
//TODO:
     if (size == 0)
    {
        mm_free(oldptr);
        return NULL;
    }
    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = GET_SIZE(HDRP(oldptr))-DSIZE;
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

//////////////////////////////////////////////////////////////////////////////////////////
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp))); // 이전 블럭이 할당 됨
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블럭이 할당 됨
    size_t size = GET_SIZE(HDRP(bp)); // 헤더 블럭의 크기를 가져옴 

    if (prev_alloc && next_alloc) 
    {// case 1
        return bp;
    }
    else if (prev_alloc && !next_alloc)
    { // case 2
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc)
    { // case 3
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        
        bp = PREV_BLKP(bp);
    }
    else
    { // case 4
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

//////////////////////////////////////////////////////////////////////////////////////////

static void *find_fit(size_t asize)
{
    // 1. first fit
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            return bp;
        }
    }
    return NULL;

    // 2. next fit
    /* 

     */


    // 3. best fit

}

//////////////////////////////////////////////////////////////////////////////////////////
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}