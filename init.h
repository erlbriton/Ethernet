#ifndef INIT_H
#define INIT_H
#include "includes.h"
extern void Init(void);
extern void InitClock(void);
extern Int32U SYS_GetFsclk(void);
extern Int32U SYS_GetFpclk(Int32U Periphery);
extern void GpioInit(void);
extern void VIC_Init(void);
extern void _VIC_SetVectoredIRQ(u32 pIRQSub, unsigned int Priority,
                               unsigned int VicIntSource);

#endif