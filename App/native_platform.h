#ifndef LEKAK_NATIVE_PLATFORM_H
#define LEKAK_NATIVE_PLATFORM_H
#include <stddef.h>
#include <stdint.h>
/* Install before starting the full engine. Callbacks run on its worker;
 * UIKit code must copy frames and dispatch UI work to the main queue.
 * This is independent of the experimental MIPS interpreter. */
typedef struct LekakNativeServices {
    void *context;
    int (*open)(void *);
    void (*frame)(void *,const uint32_t *,int,int);
    void (*error)(void *,const char *,const char *);
    int (*audio)(void *,void (*)(int16_t *,size_t));
    void (*pump)(void *);
} LekakNativeServices;
int LekakNative_Install(const LekakNativeServices *);
void LekakNative_SetPad(unsigned port,uint16_t bits,int connected);
void LekakNative_ReleasePads(void);
/* Audio and engine must stop before clearing the services. */
void LekakNative_Clear(void);
#endif
