
#include <stdio.h>   /* for size_t */

#ifndef GC_OPT_MULTITHREAD
#define GC_OPT_MULTITHREAD 1 /* set to 0 to disable multi-threading */
#endif

#ifndef GC_OPT_INCREMENTAL
#define GC_OPT_INCREMENTAL 1 /* set to 1 to enable incremental marking */
#endif

#ifndef GC_OPT_FINALIZING
#define GC_OPT_FINALIZING  0 /* set to 0 to disable finalizers & weak refs */
#endif

#define GC_MASK(x, n) ((x) << (n))

#define GC_OPT (GC_MASK(GC_OPT_MULTITHREAD, 0) |\
                GC_MASK(GC_OPT_INCREMENTAL, 1) |\
                GC_MASK(GC_OPT_FINALIZING & GC_OPT_MULTITHREAD, 2)) 

#define GC_MULTITHREAD (GC_OPT & 0x01) >> 0 
#define GC_INCREMENTAL (GC_OPT & 0x02) >> 1
#define GC_FINALIZING  (GC_OPT & 0x04) >> 2

#define GC_PRINT(OPT) printf(#OPT "=%d\n", OPT)

int main(void) {
    GC_PRINT(GC_MULTITHREAD); 
    GC_PRINT(GC_INCREMENTAL); 
    GC_PRINT(GC_FINALIZING); 
}
