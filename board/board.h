/***************************************************************************
 **
 **    This file defines the board specific definition
 **
 **    Used with ARM IAR C/C++ Compiler and Assembler.
 **
 **    (c) Copyright IAR Systems 2005
 **
 **    $Revision: 1.4 $
 **
 ***************************************************************************/
#ifndef __BOARD_H
#define __BOARD_H

#include <inarm.h>
#include "arm_comm.h"

#define I_RC_OSC_FREQ   (4MHZ)
#define MAIN_OSC_FREQ   (22118400UL)
#define RTC_OSC_FREQ    (32768UL)


// PCLK offset
#define WDT_PCLK_OFFSET     0
#define TIMER0_PCLK_OFFSET  2
#define TIMER1_PCLK_OFFSET  4
#define UART0_PCLK_OFFSET   6
#define UART1_PCLK_OFFSET   8
#define PWM0_PCLK_OFFSET    10
#define PWM1_PCLK_OFFSET    12
#define I2C0_PCLK_OFFSET    14
#define SPI_PCLK_OFFSET     16
#define RTC_PCLK_OFFSET     18
#define SSP1_PCLK_OFFSET    20
#define DAC_PCLK_OFFSET     22
#define ADC_PCLK_OFFSET     24
#define CAN1_PCLK_OFFSET    26
#define CAN2_PCLK_OFFSET    28
#define ACF_PCLK_OFFSET     30
#define BAT_RAM_PCLK_OFFSET 32
#define GPIO_PCLK_OFFSET    34
#define PCB_PCLK_OFFSET     36
#define I2C1_PCLK_OFFSET    38
//#define                   40
#define SSP0_PCLK_OFFSET    42
#define TIMER2_PCLK_OFFSET  44
#define TIMER3_PCLK_OFFSET  46
#define UART2_PCLK_OFFSET   48
#define UART3_PCLK_OFFSET   50
#define I2C2_PCLK_OFFSET    52
#define I2S_PCLK_OFFSET     54
#define MCI_PCLK_OFFSET     56
//#define                   58
#define PCLK_PCLK_OFFSET    60
//#define                   62

#define IRQ_FLAG            0x80
#define FIQ_FLAG            0x40

/*************************************************************************
 * Function Name: restore_IRQ
 * Parameters: unsigned long IFlag
 * Return: void
 * Description: Restore I flag state
 *
 *************************************************************************/
inline
__arm void restore_IRQ(unsigned long IFlag)
{
unsigned long tmp;
  tmp=__get_CPSR();
  __set_CPSR(tmp & (IFlag | ~IRQ_FLAG));
}

 /*************************************************************************
 * Function Name: disable_IRQ
 * Parameters:
 * Return: unsigned long
 * Description: Disable IRQ and return previous state state of flags I
 *
 *************************************************************************/
inline
__arm unsigned long disable_IRQ(void)
{
unsigned long tmp;
  tmp=__get_CPSR();
  __set_CPSR(tmp | IRQ_FLAG);
  return tmp & IRQ_FLAG;
}

 /*************************************************************************
 * Function Name: restore_interrupts
 * Parameters: unsigned long IFlag
 * Return: void
 * Description: Restore F,I flag state
 *
 *************************************************************************/
inline
__arm void restore_interrupts(unsigned long IFlag)
{
unsigned long tmp;
  tmp=__get_CPSR();
  __set_CPSR(tmp & (IFlag | ~(IRQ_FLAG | FIQ_FLAG)));
}

 /*************************************************************************
 * Function Name: disable_interrupts
 * Parameters:
 * Return: unsigned long
 * Description: Disable interrupts and return previous state state of flags I
 *
 *************************************************************************/
inline
__arm unsigned long disable_interrupts(void)
{
unsigned long tmp;
  tmp=__get_CPSR();
  __set_CPSR(tmp | IRQ_FLAG | FIQ_FLAG);
  return tmp;
}

#endif //__BOARD_H

