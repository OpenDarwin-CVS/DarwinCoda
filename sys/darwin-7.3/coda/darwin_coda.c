#include <mach/mach_types.h>
kern_return_t darwin_coda_start (kmod_info_t * ki, void * d);
kern_return_t darwin_coda_stop (kmod_info_t * ki, void * d);

kern_return_t darwin_coda_start (kmod_info_t * ki, void * d) {
    return KERN_SUCCESS;
}


kern_return_t darwin_coda_stop (kmod_info_t * ki, void * d) {
    return KERN_SUCCESS;
}
