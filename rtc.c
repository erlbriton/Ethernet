#include "rtc.h"
#include "ramdata.h"

//прерывания RTC используются для файловой системы, отсчёт выдержек времени
//примерно равных 0,001 сек
__irq __nested __arm  void RTC_ISR(void) {
  static u8 delay = 100;
  u8 i = ILR;
    //ILR = ILR_RTCCIF;//сброс флага прерывания
    ILR = 0x07; //сброс всех флагов прерываний
    if (i & ILR_RTSSF) {
      __enable_interrupt(); // handles nested interrupt
      //формирование 10Гц временных интервалов
      if (delay == 0) {
        TECH_10Hz_ = TRUE;
        delay = 100;
      }
      else {
        delay--;
      }
    }
    VICADDRESS = 0;//
}

/*****************************************************************************
** Function name:   RTCInit
** Descriptions:    Initialize RTC timer
** parameters:      None
** Returned value:    None
*****************************************************************************/
void RTCInit( void )
{
  PCLKSEL1_bit.PCLK_BAT_RAM = 1;
  PCONP_bit.PCRTC = 1;
  CIIR = 0;
  CCR = 0;
  CCR_bit.CLKEN  = 0;    // RTC_Disable
  CCR_bit.CLKSRC = 1;    // External 32kHz
  CCR_bit.CTCRST = CCR_bit.CTTEST = 0; // test disable and reset
  //ILR = ILR_RTCCIF;
  CISS = 1;//An interrupt is generated on every 32 counts of the Clock Tick Counter.
           //At 32.768 kHz, this generates an interrupt approximately every 977 microseconds.
  CISS |= 0x80;//SUBSECENA=1 теперь будет прерываться каждые 977uS
  //ILR = ILR_RTSSF;//разрешить прерывания с частотой менее 1 секунды
  _VIC_SetVectoredIRQ((u32)&RTC_ISR,10,VIC_RTC);
  VICINTENABLE |= 1<< VIC_RTC;
}

/*****************************************************************************
** Function name:   RTCStart
** Descriptions:    Start RTC timer
** parameters:      None
** Returned value:    None
*****************************************************************************/
void RTCStart( void )
{
  CCR_bit.CLKEN  = 1;    // RTC_Enable
}

/*****************************************************************************
** Function name:   RTCStop
** Descriptions:    Stop RTC timer
** parameters:      None
** Returned value:    None
*****************************************************************************/
void RTCStop( void )
{
    /*--- Stop RTC counters ---*/
    CCR &= ~CCR_CLKEN;
    return;
}

/*****************************************************************************
** Function name:   RTC_CTCReset
** Descriptions:    Reset RTC clock tick counter
** parameters:      None
** Returned value:    None
*****************************************************************************/
void RTC_CTCReset( void )
{
    /*--- Reset CTC ---*/
    CCR |= CCR_CTCRST;
    return;
}

/*****************************************************************************
** Function name:   RTCSetTime
** Descriptions:    Setup RTC timer value
** parameters:      None
** Returned value:    None
*****************************************************************************/
void RTCSetTime( void )
{
    SEC = RAM_DATA.RTC.RTC_Sec;
    MIN = RAM_DATA.RTC.RTC_Min;
    HOUR = RAM_DATA.RTC.RTC_Hour;
    DOM = RAM_DATA.RTC.RTC_Mday;
    DOW = RAM_DATA.RTC.RTC_Wday;
    DOY = RAM_DATA.RTC.RTC_Yday;
    MONTH = RAM_DATA.RTC.RTC_Mon;
    YEAR = RAM_DATA.RTC.RTC_Year;
    return;
}

/*****************************************************************************
** Function name:   RTCGetTime
** Descriptions:    Get RTC timer value
** parameters:      None
** Returned value:    The data structure of the RTC time table
*****************************************************************************/
void RTCGetTime( void )
{
    RAM_DATA.RTC.RTC_USec = CTC;
    RAM_DATA.RTC.RTC_Sec = SEC;
    RAM_DATA.RTC.RTC_Min = MIN;
    RAM_DATA.RTC.RTC_Hour = HOUR;
    RAM_DATA.RTC.RTC_Mday = DOM;
    RAM_DATA.RTC.RTC_Wday = DOW;
    RAM_DATA.RTC.RTC_Yday = DOY;
    RAM_DATA.RTC.RTC_Mon = MONTH;
    RAM_DATA.RTC.RTC_Year = YEAR;
}

// FatFs module. Any valid time must be returned even if
// the system does not support an RTC.
// This function is not required in read-only cfg.
DWORD get_fattime (void) {
  // Get local time
  RTCGetTime();
  // Pack date and time into a DWORD variable
  return ((DWORD)(YEAR - 1980) << 25)
	| ((DWORD)MONTH << 21)
	| ((DWORD)DOM << 16)
	| ((DWORD)HOUR << 11)
	| ((DWORD)MIN << 5)
	| ((DWORD)SEC >> 1);
}
