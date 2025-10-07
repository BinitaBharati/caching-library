#include <jemalloc/jemalloc.h>
#include <stdio.h>
#include <malloc.h>

/**
 * Prints memory usage stats when jemalloc is used as memory allocator.
 * Your existing code that uses malloc and free calls need not be changed.
 * You can make jemalloc's corresponding implementations to be used at run time
 * by running the C code like below:
 * LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libjemalloc.so.2 ./my_program
 * 
 * You can also link jemalloc at build time without changing your code this way:
 * 
 */
void print_jemalloc_stats() {
    malloc_stats_print(NULL, NULL, NULL);
}

/**
 * Prints memory usage stats when default glibc malloc is used as memory allocator
 */
void print_glibc_malloc_stats2() {
    struct mallinfo2 mi = mallinfo2();
    printf("[glibc mallinfo2]\n");
    printf("  arena:     %zu MB\n", mi.arena / (1024*1024));
    printf("  uordblks:  %zu MB (allocated)\n", mi.uordblks / (1024*1024));
    printf("  fordblks:  %zu MB (free)\n", mi.fordblks / (1024*1024));
    printf("  keepcost:  %zu MB (releasable)\n", mi.keepcost / (1024*1024));
}