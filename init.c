//инициализаци€ переферии
#include "init.h"
//описани€
void Init(void);
void InitClock(void);
Int32U SYS_GetFsclk(void);
Int32U SYS_GetFpclk(Int32U Periphery);
void GpioInit(void);
void VIC_Init(void);
void _VIC_SetVectoredIRQ(u32 pIRQSub, unsigned int Priority,
                               unsigned int VicIntSource);
//реализаци€

/*************************************************************************
 * Function Name: InitClock
 * Parameters: void
 * Return: void
 *
 * Description: Initialize PLL and clocks' dividers. Hclk - 288MHz,
 * Cclk- 48MHz, Usbclk - 48MHz
 *
 *************************************************************************/
/* Fcck = 50Mhz, Fosc = 300Mhz, and USB 48Mhz */
#define PLL_MValue	24
#define PLL_NValue	1
#define CCLKDivValue	6

void InitClock(void)
{
  // 1. Init OSC
  SCS_bit.OSCRANGE = 1;//внешний кварц 22.1184ћ√ц (1 = 15...24ћгц, 0 = 1...20ћ√ц)
  SCS_bit.OSCEN = 1;//кварц подключен к выводам XTAL
  // 2.  Wait for OSC ready
  while(!SCS_bit.OSCSTAT);//когда приготовитс€, пойду дальше
  // 3. Disconnect PLL
  PLLCON_bit.PLLC = 0;
  PLLFEED = 0xAA;
  PLLFEED = 0x55;
  // 4. Disable PLL
  PLLCON_bit.PLLE = 0;
  PLLFEED = 0xAA;
  PLLFEED = 0x55;
  // 5. Select source clock for PLL
  CLKSRCSEL_bit.CLKSRC = 1; // Selects the main oscillator as a PLL clock source.

  // 6. Set PLL settings 265,4208ћ√ц
  PLLCFG_bit.MSEL = 6-1;
  PLLCFG_bit.NSEL = 1-1;
  PLLFEED = 0xAA;
  PLLFEED = 0x55;
  // 7. Enable PLL
  PLLCON_bit.PLLE = 1;
  PLLFEED = 0xAA;
  PLLFEED = 0x55;
  // 8. Wait for the PLL to achieve lock
  while(!PLLSTAT_bit.PLOCK);
  // 9. Set clk divider settings
  CCLKCFG   = 4-1;            // 1/4 Fpll - 66,3552 MHz
  USBCLKCFG = 6-1;            // 1/6 Fpll - 44,2368 MHz
  PCLKSEL0 = PCLKSEL1 = 0;    // other peripherals - 22,1184MHz (CPU_CLK/4)
  // 10. Connect the PLL
  PLLCON_bit.PLLC = 1;
  PLLFEED = 0xAA;
  PLLFEED = 0x55;
}

/*************************************************************************
 * Function Name: SYS_GetFsclk
 * Parameters: none
 * Return: Int32U
 *
 * Description: return Sclk [Hz]
 *
 *************************************************************************/
Int32U SYS_GetFsclk(void)
{
Int32U Mul = 1, Div = 1, Osc, Fsclk;
  if(PLLSTAT_bit.PLLC)
  {
    // when PLL is connected
    Mul = PLLSTAT_bit.MSEL + 1;
    Div = PLLSTAT_bit.NSEL + 1;
  }

  // Find clk source
  switch(CLKSRCSEL_bit.CLKSRC)
  {
  case 0:
    Osc = I_RC_OSC_FREQ;
    break;
  case 1:
    Osc = MAIN_OSC_FREQ;
    break;
  case 2:
    Osc = RTC_OSC_FREQ;
    break;
  default:
    Osc = 0;
  }
  // Calculate system frequency
  Fsclk = Osc*Mul*2;
  Fsclk /= Div*(CCLKCFG+1);
  return(Fsclk);
}

/*************************************************************************
 * Function Name: SYS_GetFpclk
 * Parameters: Int32U Periphery
 * Return: Int32U
 *
 * Description: return Pclk [Hz]
 *
 *************************************************************************/
Int32U SYS_GetFpclk(Int32U Periphery)
{
Int32U Fpclk;
pInt32U pReg = (pInt32U)((Periphery < 32)?&PCLKSEL0:&PCLKSEL1);

  Periphery  &= 0x1F;   // %32
  Fpclk = SYS_GetFsclk();
  // find peripheral appropriate periphery divider
  switch((*pReg >> Periphery) & 3)
  {
  case 0:
    Fpclk /= 4;
    break;
  case 1:
    break;
  case 2:
    Fpclk /= 2;
    break;
  default:
    Fpclk /= 8;
  }
  return(Fpclk);
}
/*************************************************************************
 * Function Name: GpioInit
 * Parameters: void
 * Return: void
 *
 * Description: Reset all GPIO pins to default: primary function
 *
 *************************************************************************/
void GpioInit(void)
{
  // Set to inputs
  IO0DIR  = \
  IO1DIR  = \
  FIO0DIR = \
  FIO1DIR = \
  FIO2DIR = \
  FIO3DIR = \
  FIO4DIR = 0;

  FIO2DIR = 0xffffffff;
  // Enable Fast GPIO0,1
  SCS_bit.GPIOM = 1;

  // clear mask registers
  FIO0MASK =\
  FIO1MASK =\
  FIO2MASK =\
  FIO3MASK =\
  FIO4MASK = 0;

  // Reset all GPIO pins to default primary function
  PINSEL0 =\
  PINSEL1 =\
  PINSEL2 =\
  PINSEL3 =\
  PINSEL4 =\
  PINSEL5 =\
  PINSEL6 =\
  PINSEL7 =\
  PINSEL8 =\
  PINSEL9 =\
  PINSEL10= 0;
}

/*************************************************************************
 * Function Name: VIC_Init
 * Parameters: void
 * Return: void
 *
 * Description: Initialize VIC
 *
 *************************************************************************/
void VIC_Init(void)
{
volatile unsigned long * pVecAdd, *pVecCntl;
int i;
  // Assign all interrupt channels to IRQ
  VICINTSELECT  =  0;
  // Disable all interrupts
  VICINTENCLEAR = 0xFFFFFFFF;
  // Clear all software interrupts
  VICSOFTINTCLEAR = 0xFFFFFFFF;
  // VIC registers can be accessed in User or privileged mode
  VICPROTECTION = 0;
  // Clear interrupt
  VICADDRESS = 0;

  // Clear address of the Interrupt Service routine (ISR) for vectored IRQs
  // and disable all vectored IRQ slots
  for(i = 0,  pVecCntl = &VICVECTPRIORITY0, pVecAdd = &VICVECTADDR0; i < 32; ++i)
  {
    *pVecCntl++ = *pVecAdd++ = 0;
  }
}

/*************************************************************************
 * Function Name: VIC_SetVectoredIRQ
 * Parameters:  void(*pIRQSub)()
 *              unsigned int VicIrqSlot
 *              unsigned int VicIntSouce
 *
 * Return: void
 *
 * Description:  Init vectored interrupts
 *
 *************************************************************************/
void VIC_SetVectoredIRQ(void(*pIRQSub)(), unsigned int Priority,
                               unsigned int VicIntSource)
{
unsigned long volatile *pReg;
  // load base address of vectored address registers
  pReg = &VICVECTADDR0;
  // Set Address of callback function to corresponding Slot
  *(pReg+VicIntSource) = (u32)pIRQSub;
  // load base address of ctrl registers
  pReg = &VICVECTPRIORITY0;
  // Set source channel and enable the slot
  *(pReg+VicIntSource) = Priority;
  // Clear FIQ select bit
  VICINTSELECT &= ~(1<<VicIntSource);
}

void _VIC_SetVectoredIRQ(u32 pIRQSub, unsigned int Priority,
                               unsigned int VicIntSource)
{
unsigned long volatile *pReg;
  // load base address of vectored address registers
  pReg = &VICVECTADDR0;
  // Set Address of callback function to corresponding Slot
  *(pReg+VicIntSource) = pIRQSub;
  // load base address of ctrl registers
  pReg = &VICVECTPRIORITY0;
  // Set source channel and enable the slot
  *(pReg+VicIntSource) = Priority;
  // Clear FIQ select bit
  VICINTSELECT &= ~(1<<VicIntSource);
}

void Init(void) {
  // MAM init
  MAMCR_bit.MODECTRL = 0;
  MAMTIM_bit.CYCLES  = 3;   // FCLK > 40 MHz
  MAMCR_bit.MODECTRL = 2;   // MAM functions fully enabled
  // Init clock
  InitClock();
  // Init GPIO
  GpioInit();
    //
    FIO0SET |= WRK_LED | ETHERNET_LED| RS485_JP1_LED | RS485_JP2_LED;
    FIO0DIR |= WRK_LED | ETHERNET_LED | RS485_JP1_LED | RS485_JP2_LED;
    //
    FIO2DIR &= ~EINT0;//на ввод
  // Init VIC
  VIC_Init();
  //мне не до экономии
  PCON |=0x8;
  PCON_bit.BOGD = 1;
}