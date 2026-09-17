#ifndef RTC_H
#define RTC_H
#include "includes.h"


#define PREINT_RTC  0x000001C8 //Prescaler value, integer portion, PCLK = 15Mhz
#define PREFRAC_RTC 0x000061C0 // Prescaler value, fraction portion, PCLK = 15Mhz
#define ILR_RTCCIF  0x01 //When one, the Counter Increment Interrupt block generated an interrupt.
                         //Writing a one to this bit location clears the counter increment interrupt.
#define ILR_RTSSF   0x04 //When one, the Counter Increment Sub-Seconds interrupt is generated.
                         //The interrupt rate is determined by the CISS register.
#define CCR_CLKEN   0x01
#define CCR_CTCRST  0x02

typedef struct RTCTime{//64 байта - никуда не годится!!
    u32 RTC_Sec;     /* Second value - [0,59] */
    u32 RTC_Min;     /* Minute value - [0,59] */
    u32 RTC_Hour;    /* Hour value - [0,23] */
    u32 RTC_Mday;    /* Day of the month value - [1,31] */
    u32 RTC_Mon;     /* Month value - [1,12] */
    u32 RTC_Year;    /* Year value - [0,4095] */
    u32 RTC_Wday;    /* Day of week value - [0,6] */
    u32 RTC_Yday;    /* Day of year value - [1,365] */
} RTCTime;

typedef struct TRTCTime{//12 байт
    u16 RTC_USec; //микросекунды 0...65526
    u8  RTC_Sec;   // Second value - [0,59]
    u8  RTC_Min;   // Minute value - [0,59]
    u8  RTC_Hour;  // Hour value - [0,23]
    u8  RTC_Mday;  // Day of the month value - [1,31]
    u16 RTC_Mon;  // Month value - [1,12]
    u16 RTC_Year; // Year value - [0,4095]
    u16 RTC_Wday; // Day of week value - [0,6]
    u16 RTC_Yday; // Day of year value - [1,365]
} TRTCTime;

extern void RTCInit(void);
extern void RTCStart(void);
extern void RTCStop(void);
extern void RTC_CTCReset(void);
extern void RTCSetTime(void);
extern void RTCGetTime(void);
extern DWORD get_fattime (void);

#endif
