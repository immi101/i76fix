#include <unistd.h>

/*
 * sample C code, to derive assembly code from */


int (*pGetModuleHandle)(char *) = (void*)0x4bc0ec;
void* (*pGetProcAddress)(int, char*) = (void*)0x4BC0A0;
int (*pGetTickCount)(void) = (void*)0x4bc100;
int *LastTick = (void*)0x4C25F0;
void (*pSleep)(int) = (void*)0x4C25EC;

void draw_delay(void)
{
    int cur = pGetTickCount();
    if(*LastTick)
    {
        int diff = cur - *LastTick;
        if(diff < 32 )
        {
            if( pSleep == NULL)
            {
                pSleep = pGetProcAddress( pGetModuleHandle("kernel32"), "Sleep");
            }
            pSleep( 33 - diff );
            cur = pGetTickCount();
        }
    }
    *LastTick = cur;
}
