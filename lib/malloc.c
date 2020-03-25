
/*
 * copied and modified from mm.c in my malloc lab handin
 *
 * Name: Yang Jiahong
 * StudentID: 1600012769
 *
 * Description:
 *     Maintain free blocks in a splay (for big blocks) and a segregated list
 *     (for small blocks), and allocate new blocks using best-fit strategy.
 *     An optimization is to set a minimum size when extending the heap, which
 *     reduces the time of system calls.
 */
#include <string.h>
#include <tinylib.h>
#include <syscall.h>

typedef unsigned long long uLL;
typedef unsigned int uint;

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define MALLOC_DEBUG line. */
// #define MALLOC_DEBUG
#ifdef MALLOC_DEBUG
#define dbg_printf(...) printf(__VA_ARGS__), fflush(stdout)
#define checker(...) mm_checkheap(__VA_ARGS__)
#else
#define dbg_printf(...)
#define checker(...)
#endif

/* double word (8) alignment */
#define ALIGNMENT 8
#define MIN_BLOCK_SIZE 16
#define MIN_TREE_BLOCK_SIZE 40
#define MIN_NEW_SIZE 128
#define MAX_DEPTH 300
#define HEAP_START 0x800000000ULL

static char *heap = (char*)HEAP_START;
static char *mem_brk = (char*)HEAP_START;

static inline void *mem_sbrk(size_t incr) {
    mem_brk += incr;
	return sys_sbrk(incr);
}

static inline void *mem_heap_lo(){
	return (void *)heap;
}

static inline void *mem_heap_hi(){
	return (void *)(mem_brk - 1);
}

// above is some helpers

#define max(a, b) ((a) > (b) ? (a) : (b))

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((uint)(p) + (ALIGNMENT-1)) & ~0x7)

/* some macros and small functions to maintain information in a block */
#define HEAD(p) (*((uint*)(p) - 1))
#define SIZE(p) (HEAD(p) & ~0x7)
#define FOOT(p) (*((uint*)((void*)(p) + SIZE(p)) - 2))
#define CHK_USED(p) (HEAD(p) & 0x1)
#define CHK_PREC_USED(p) ((HEAD(p) >> 1) & 0x1)

#define SET_USED(p) (HEAD(p) |= 0x1)
#define SET_UNUSED(p) (HEAD(p) &= ~0x1)
#define SET_PREC_USED(p) (HEAD(p) |= 0x2)
#define SET_PREC_UNUSED(p) (HEAD(p) &= ~0x2)

inline static void SET_SIZE(void *p, uint size)
{
    HEAD(p) = (size & ~0x7) | (HEAD(p) & 0x7);
    if (!CHK_USED(p)) FOOT(p) = size;
}

#define GET_PREC_PTR(p) ((void*)(p) - *((uint*)(p) - 2))

#define SET_SON(p, i, sp) \
    (*((uint*)(p) + (i)) = (uint)((size_t)sp & 0xFFFFFFFFLL))

inline static void *GET_SON(void *p, uint i)
{
    i = *((uint*)p + i);
    if (i == 0) return NULL;
    return (void*)(HEAP_START | i);
}

/* global variables */
static void *root; // the root of the splay tree
static void **segList; // the small-size segregated list

#ifdef MALLOC_DEBUG
int op_counter;
#endif

/**
 * Splay a node to the root of the given path
 * @param depth     the length of the path
 * @param path_list nodes in the path
 * @param dir_list  determine if a node is a left son or a right son
 */
static void splay(int depth, void **path_list, int *dir_list)
{
    if (depth == 0) return;

    dbg_printf("  depth: %d\n", depth);

    void *x = path_list[depth - 1];
    while (depth > 2)
    {
        int dir = dir_list[depth - 1];
        void *y = path_list[depth - 2];
        void *z = path_list[depth - 3];
        if (dir == dir_list[depth - 2])
        {
            SET_SON(z, dir, GET_SON(y, !dir));
            SET_SON(y, !dir, z);
            SET_SON(y, dir, GET_SON(x, !dir));
            SET_SON(x, !dir, y);
        }
        else
        {
            SET_SON(y, dir, GET_SON(x, !dir));
            SET_SON(z, !dir, GET_SON(x, dir));
            SET_SON(x, dir, z);
            SET_SON(x, !dir, y);
        }
        depth -= 2;
    }
    if (depth > 1)
    {
        int dir = dir_list[depth - 1];
        void *y = path_list[depth - 2];
        SET_SON(y, dir, GET_SON(x, !dir));
        SET_SON(x, !dir, y);
    }
    root = x;
}

/**
 * compare two nodes in the splay
 * @param  ptr1 a node
 * @param  ptr2 a node
 * @return      -1 represent ptr1 < ptr2, 1 otherwise
 */
inline static int comp(void *ptr1, void *ptr2)
{
    uint s1 = SIZE(ptr1), s2 = SIZE(ptr2);
    if (s1 != s2)
        return s1 < s2 ? -1 : 1;
    return ptr1 < ptr2 ? -1 : 1;
}

/**
 * insert a free block to the splay or the segregated list,
 *     depend on the size of the block
 * @param ptr the pointer to the block
 */
static void insert_block(void *ptr)
{
    // small block insert to the segregated list
    if (SIZE(ptr) < MIN_TREE_BLOCK_SIZE)
    {
        uint index;
        for (index = 0; index < 3; index++)
            if (SIZE(ptr) == MIN_BLOCK_SIZE + ALIGNMENT * index)
                break;
        SET_SON(ptr, 0, NULL);
        SET_SON(ptr, 1, segList[index]);
        if (segList[index] != NULL)
            SET_SON(segList[index], 0, ptr);
        segList[index] = ptr;
        return;
    }

    // big block insert to the splay
    int depth = 0, dir_list[MAX_DEPTH];
    void *path_list[MAX_DEPTH];
    void *p = root, *par = NULL;
    while (p != NULL)
    {
        par = p;
        path_list[depth++] = p;
        if (comp(ptr, p) < 0)
        {
            p = GET_SON(p, 0);
            dir_list[depth] = 0;
        }
        else
        {
            p = GET_SON(p, 1);
            dir_list[depth] = 1;
        }
    }
    SET_SON(ptr, 0, NULL);
    SET_SON(ptr, 1, NULL);
    if (par != NULL)
    {
        SET_SON(par, dir_list[depth], ptr);
        path_list[depth++] = ptr;
        splay(depth, path_list, dir_list);
    }
    else
    {
        root = ptr;
    }
}

/**
 * delete a free block to the splay or the segregated list,
 *     depend on the size of the block
 * @param ptr the pointer to the block
 */
static void delete_block(void *ptr)
{
    // small block is in the segregated list
    if (SIZE(ptr) < MIN_TREE_BLOCK_SIZE)
    {
        uint index;
        for (index = 0; index < 3; index++)
            if (SIZE(ptr) == MIN_BLOCK_SIZE + ALIGNMENT * index)
                break;
        if (segList[index] == ptr)
        {
            void *succ = GET_SON(ptr, 1);
            if (succ != NULL) SET_SON(succ, 0, NULL);
            segList[index] = succ;
        }
        else
        {
            void *prec = GET_SON(ptr, 0);
            void *succ = GET_SON(ptr, 1);
            SET_SON(prec, 1, succ);
            if (succ != NULL) SET_SON(succ, 0, prec);
        }
        return;
    }

    // big block in the splay
    int depth = 0, dir_list[MAX_DEPTH];
    void *path_list[MAX_DEPTH];
    void *p = root;
    while (p != NULL)
    {
        path_list[depth++] = p;
        if (p == ptr) break;
        if (comp(ptr, p) < 0)
        {
            p = GET_SON(p, 0);
            dir_list[depth] = 0;
        }
        else
        {
            p = GET_SON(p, 1);
            dir_list[depth] = 1;
        }
    }
    if (GET_SON(p, 0) == NULL)
    {
        if (depth > 1)
            SET_SON(path_list[depth - 2], dir_list[depth - 1], GET_SON(p, 1));
        else
            root = GET_SON(p, 1);
        splay(depth - 1, path_list, dir_list);
        return;
    }
    if (GET_SON(p, 1) == NULL)
    {
        if (depth > 1)
            SET_SON(path_list[depth - 2], dir_list[depth - 1], GET_SON(p, 0));
        else
            root = GET_SON(p, 0);
        splay(depth - 1, path_list, dir_list);
        return;
    }
    splay(depth, path_list, dir_list);
    p = GET_SON(root, 0);
    if (p != NULL)
    {
        depth = 0;
        while (p != NULL)
        {
            path_list[depth++] = p;
            dir_list[depth] = 1;
            p = GET_SON(p, 1);
        }
        p = GET_SON(root, 1);
        splay(depth, path_list, dir_list);
        SET_SON(root, 1, p);
    }
    else
    {
        root = GET_SON(root, 1);
    }
}

/**
 * find best-fit block
 * @param  asize the minimum size of the free block
 * @return       the pointer to the best-fit free block
 */
static void *find_best_fit(uint asize)
{
    // first try small blocks
    if (asize < MIN_TREE_BLOCK_SIZE)
    {
        uint index;
        for (index = 0; index < 3; index++)
            if (asize <= MIN_BLOCK_SIZE + ALIGNMENT * index
                    && segList[index] != NULL)
                break;
        if (index < 3)
            return segList[index];
    }

    // then try blocks in the splay
    int depth = 0, dir_list[MAX_DEPTH];
    void *path_list[MAX_DEPTH];
    void *p = root, *candidate = NULL;
    while (p != NULL)
    {
        path_list[depth++] = p;
        if (SIZE(p) >= asize)
        {
            candidate = p;
            if ((uLL)SIZE(p) * 100 <= (uLL)asize * 111) break;
            // if (SIZE(p) <= (uint)(asize * 1.11)) break;
            p = GET_SON(p, 0);
            dir_list[depth] = 0;
        }
        else
        {
            p = GET_SON(p, 1);
            dir_list[depth] = 1;
        }
    }
    splay(depth, path_list, dir_list);
    return candidate;
}

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void)
{
#ifdef MALLOC_DEBUG
    op_counter = 0;
#endif
    void *p = mem_sbrk(32) + 32;
    // set boundary
    HEAD(p) = 0;
    SET_PREC_USED(p);
    // initialize global variables
    root = NULL;
    segList = mem_heap_lo();
    for (int i = 0; i < 3; i++)
        segList[i] = NULL;
    return 0;
}

/*
 * malloc
 */
void *malloc(size_t size)
{
    static int initialized = 0;
    if (!initialized) {
        initialized = 1;
        mm_init();
    }

    uint asize = ALIGN(size + sizeof(uint));
    if (asize < MIN_BLOCK_SIZE)
        asize = MIN_BLOCK_SIZE;

    dbg_printf("op:%d\tmalloc: %lu(req)\t-> %d(real) => ",
               ++op_counter, size, asize);

    dbg_printf("find! %llx\n", segList[1]);
    void *newptr = find_best_fit(asize);
    if (newptr != NULL) // find an available free block!
    {
        delete_block(newptr);
        SET_USED(newptr);
        uint remainsize = SIZE(newptr) - asize;
        if (remainsize >= MIN_BLOCK_SIZE) // split the block
        {
            SET_SIZE(newptr, asize);
            void *nxtptr = newptr + asize;
            SET_UNUSED(nxtptr);
            SET_SIZE(nxtptr, remainsize);
            insert_block(nxtptr);
        }
        SET_PREC_USED(newptr + SIZE(newptr));
    }
    else // extend the heap
    {
        // check if the last block is free
        uint last_free_size = 0;
        void *mem_hi = mem_heap_hi() + 1;
        if (!CHK_PREC_USED(mem_hi))
        {
            mem_hi = GET_PREC_PTR(mem_hi);
            last_free_size = SIZE(mem_hi);
            delete_block(mem_hi);
        }
        // extend the heap
        uint newsize = max(asize - last_free_size, MIN_NEW_SIZE);
        newptr = mem_sbrk(newsize) - last_free_size;
        newsize += last_free_size;
        if (newsize - asize >= MIN_BLOCK_SIZE)
        {
            // handle the header of this new block
            SET_USED(newptr);
            SET_SIZE(newptr, asize);
            // handle the free block
            void *nxtptr = newptr + asize;
            SET_UNUSED(nxtptr);
            SET_SIZE(nxtptr, newsize - asize);
            SET_PREC_USED(nxtptr);
            insert_block(nxtptr);
            // handle the header of the tail boundary
            nxtptr = newptr + newsize;
            SET_USED(nxtptr);
            SET_SIZE(nxtptr, 0);
            SET_PREC_UNUSED(nxtptr);
        }
        else
        {
            asize = newsize;
            // handle the header of this new block
            SET_USED(newptr);
            SET_SIZE(newptr, asize);
            // handle the header of the tail boundary
            void *nxtptr = newptr + asize;
            SET_USED(nxtptr);
            SET_SIZE(nxtptr, 0);
            SET_PREC_USED(nxtptr);
        }
    }

    dbg_printf("%llx\n", (uLL)newptr);
    checker(0);

    return newptr;
}

/**
 * merge two adjacent free block
 * @param  ptr    the free block in the front
 * @param  nxtptr the free block following ptr
 * @return        the pointer to the merged new block
 */
inline static void *merge_block(void *ptr, void *nxtptr)
{
    uint newsize = SIZE(ptr) + SIZE(nxtptr);
    SET_SIZE(ptr, newsize);
    return ptr;
}

/*
 * free
 */
void free(void *ptr)
{
    dbg_printf("op:%d\tfree: %llx\n", ++op_counter, (uLL)ptr);
    if (ptr == NULL) return;

    SET_UNUSED(ptr);
    SET_SIZE(ptr, SIZE(ptr));
    SET_PREC_UNUSED(ptr + SIZE(ptr));
    // try to merge the last block
    if (!CHK_PREC_USED(ptr))
    {
        delete_block(GET_PREC_PTR(ptr));
        ptr = merge_block(GET_PREC_PTR(ptr), ptr);
    }
    // try to merge the next block
    void *nxtptr = ptr + SIZE(ptr);
    if (!CHK_USED(nxtptr))
    {
        delete_block(nxtptr);
        ptr = merge_block(ptr, nxtptr);
    }
    insert_block(ptr);
    checker(1);
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *oldptr, size_t size)
{
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if (size == 0)
    {
        free(oldptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if (oldptr == NULL)
    {
        return malloc(size);
    }

    newptr = malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if (!newptr)
    {
        return 0;
    }

    /* Copy the old data. */
    oldsize = SIZE(oldptr);
    if (size < oldsize) oldsize = size;
    memcpy(newptr, oldptr, oldsize);

    /* Free the old block. */
    free(oldptr);

    return newptr;
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *calloc(size_t nmemb, size_t size)
{
    uint bytes = nmemb * size;
    void *newptr;

    newptr = malloc(bytes);
    memset(newptr, 0, bytes);

    return newptr;
}

//////////////////////////
// Debug helper routine //
//////////////////////////

/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void *p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/**
 * print out the splay
 * @param p     current node
 * @param depth current depth
 */
static void print_bst(void *p, int depth)
{
    if (p == NULL) return;
    print_bst(GET_SON(p, 0), depth + 1);
    for (int i = 0; i < depth; i++)
        printf("  ");
    printf("%llx(%u:%d:%d)\n", (uLL)p,
           SIZE(p), CHK_PREC_USED(p), CHK_USED(p));
    print_bst(GET_SON(p, 1), depth + 1);
}

/*
 * mm_checkheap
 */
void mm_checkheap(int flag)
{
    printf("flag: %s\n", flag == 0 ? "malloc" : "free");
    print_bst(root, 0);
    printf("\n");
}
