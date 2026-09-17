/*************************************************************************
 *
 *    Used with ICCARM and AARM.
 *
 *    (c) Copyright IAR Systems 2006
 *
 *    File name   : LPC23xx_enet.c
 *    Description : MAC/DMA Controller with DMA (ENET) driver
 *
 *    History :
 *    1. Date        : December 14, 2006
 *       Author      : Stanimir Bonev
 *       Description : Create
 *
 *    $Revision: 1.1.2.3 $
 **************************************************************************/
#include "LPC23xx_enet.h"
#include <string.h>
#include "ramdata.h"

#pragma segment="EMAC_DMA_RAM"

#pragma location="EMAC_DMA_RAM"
#pragma data_alignment=4
__no_init EnetDmaRxDesc_t EnetDmaRx[ENET_DMA_DESC_NUMB];
#pragma location="EMAC_DMA_RAM"
#pragma data_alignment=8
__no_init EnetDmaRxStatus_t EnetDmaRxSta[ENET_DMA_DESC_NUMB];
#pragma location="EMAC_DMA_RAM"
#pragma data_alignment=4
__no_init Int8U RxBuff0[EMAC_MAX_PACKET_SIZE];
#pragma location="EMAC_DMA_RAM"
#pragma data_alignment=4
__no_init Int8U RxBuff1[EMAC_MAX_PACKET_SIZE];


#pragma location="EMAC_DMA_RAM"
#pragma data_alignment=4
__no_init EnetDmaTxDesc_t EnetDmaTx[ENET_DMA_DESC_NUMB];
#pragma location="EMAC_DMA_RAM"
#pragma data_alignment=4
__no_init EnetDmaTxStatus_t EnetDmaTxSta[ENET_DMA_DESC_NUMB];
#pragma location="EMAC_DMA_RAM"
#pragma data_alignment=4
__no_init Int8U TxBuff0[EMAC_MAX_PACKET_SIZE];
#pragma location="EMAC_DMA_RAM"
#pragma data_alignment=4
__no_init Int8U TxBuff1[EMAC_MAX_PACKET_SIZE];

static Int8U PhyAddr;

/*************************************************************************
 * Function Name:
 * Parameters: None
 *
 * Return: Boolean
 *
 * Description: Init  MAC/DMA Controller
 *
 *************************************************************************/
u8 tapdev_init_stage = tapdev_init_stage_HWINI;//стадия инициализации езернета

Boolean tapdev_init(void)
{
 //состояния цикла (для ео разделения на части) 
 #define Lnk_Est_SetTo 0 //
 #define Lnk_Est_Loop  1 //
 u32 Reg;
 static u32 to = 0;
 static u8  ProcStage = 0;//стадия процесса
 //RAM_DATA.PWM3 = tapdev_init_stage;//узнаю на какой стадии застревает
  if (tapdev_init_stage == tapdev_init_stage_HWINI) {//инициализация GPIO и Микрели
    // Pins assignment
    // PINMODE2 = 0xA02A220A;  // P1[0,1,4,6,8,9,10,14,15] disable pu/pd
    PINMODE2 = 0xA02A020A;  // P1[0,1,4,8,9,10,14,15] disable pu/pd
    PINMODE3 = 0x0000000A;  // P1[17:16] disable pu/pd  
    //  PINSEL2  = 0x50151105;  // selects P1[0,1,4,6,8,9,10,14,15] }rn dk whonb rev. '-'
    PINSEL2  = 0x50150105;  // selects P1[0,1,4,8,9,10,14,15] }rn dk whonb rev. 'A' h d`k|xe
    PINSEL3  = 0x00000005;  // selects P1[17:16]
    // clk enable
    PCONP_bit.PCENET = 1;
    POWERDOWN_bit.POWERDOWN = 0;
    // Reset entire MAC
    MAC1 = 0x0000CF00;  // reset transmit, receive function logic, reset MAC control
                        // sublayer / transmit, receive logic, reset random number
                        // generator within transmit, put all modules in reset
      
    COMMAND = 0x0038;   // reset all datapaths and host registers. Должны быть клоки от Микрели
    MAC1 = 0;
    COMMAND_bit.RMII = 1;
    SUPP = 0;
    TEST = 0;
   
    // write the station address registers
    SA0 = (UIP_ETHADDR1<<8) | UIP_ETHADDR0;
    SA1 = (UIP_ETHADDR3<<8) | UIP_ETHADDR2;
    SA2 = (UIP_ETHADDR5<<8) | UIP_ETHADDR4;

    MAXF = 0x600;   // Maximum Frame 1536 a`ir
    MCFG = 0x801C;  // MII Mgmt (MDIO) config: MDC = Host_clk(66LCv)/28 = 2.36 LCv, reset MII Mgmt
    //0x8018;  // MII Mgmt (MDIO) config: MDC = Host_clk(48LCv)/20, reset MII Mgmt
    MCMD = 0;
    // MIIM init
    MCFG_bit.RSTMIIMGMT = 0;  // qmkh peqer q MII

    // descriptors init
    MAC1_bit.RSTMCSTX = 1;    // reset MAC control sublayer / transmit logic
    COMMAND_bit.TXENABLE = 0; // disable transmit
    MAC1_bit.RSTTX = 1;       // reset transmit function logic
    COMMAND_bit.TXRESET = 1;  // transmit datapath reset
    MAC1_bit.RSTMCSTX = 0;
    MAC1_bit.RSTTX = 0;

    MAC1_bit.RE = 0;          // receive enable
    MAC1_bit.RSTMCSRX = 1;    // reset MAC control sublayer / receive logic
    COMMAND_bit.RXENABLE = 0; // disable receive
    MAC1_bit.RSTRX = 1;       // reset receive function logic
    COMMAND_bit.RXRESET = 1;  // receive datapath reset
    MAC1_bit.RSTMCSRX = 0;
    MAC1_bit.RSTRX = 0;

    RXDESCRIPTORNUMBER = ENET_DMA_DESC_NUMB-1;
    TXDESCRIPTORNUMBER = ENET_DMA_DESC_NUMB-1;
    COMMAND_bit.PASSRUNTFRAME = 1;  // opnosqj`er tpeil{ < 64 a`ir (me b{ap`q{b`er)
    COMMAND_bit.PASSRXFILTER = 1;   // g`operhr| RX-thk|rp (bqe o`jer{ ohxsrq)
    MAC1_bit.PARF = 1;              // pass all receive frames
    MAC2 = 0x30;                    // crc enable, padding enable

    CLRT_bit.RM    = 0xF;   // retransmission (on collision) maximum (qr`md`prmne gm`wemhe 0xF)
    CLRT_bit.CW    = 0x37;  // collision window (qr`md`prmne gm`wemhe 0x37)
    IPGR_bit.IPGR2 = 0x12;  // inter-packet gap (pejnlemdselne gm`wemhe 0x12)
    IPGR_bit.IPGR1 = 0x0c;  // carrier sense window (pejnlemdselne gm`wemhe 0xc)
    IPGT           = 0x12;  // nopedeker bpel lefds o`jer`lh (0x12 pejnlemds~r dk half-duplex)
    MAC2_bit.FD = COMMAND_bit.FULLDUPLEX = 0; // onksdsokejq
    SUPP_bit.SPEED = 0;     // 10 Mbps
    //готовлю переход к следующей стадии (поиск PHY address)
    tapdev_init_stage = tapdev_init_stage_PHYSRC;
    return (FALSE);
  }
  ////////////////////////////////////////////////////////////////////////////
  /* find PHY address */
  if (tapdev_init_stage == tapdev_init_stage_PHYSRC) {
    for(PhyAddr = 1; PhyAddr < 32; ++PhyAddr)
     {
       /* See Micrel PHY KS8721 Users Manual for more details */
       if((ENET_MIIReadRegister(PhyAddr, PHY_PHYIDR1) & 0xFFFF) != 0x0022)
        {continue;}
       else
         if ((ENET_MIIReadRegister(PhyAddr, PHY_PHYIDR2) & 0xFFFF) == 0x1619)
           {break;}
         else
           {continue;}
     }
    //готовлю переход к следующей стадии (поиск PHY address)
    tapdev_init_stage = tapdev_init_stage_peqerhl_PHY;
    return (FALSE);  
   }
   ////////////////////////////////////////////////////////////////////////////
   if (tapdev_init_stage == tapdev_init_stage_peqerhl_PHY){
     if(PhyAddr == 32)
       { //не прошёл инициализацию, надо вернуться к предыдущему шагу
         tapdev_init_stage = tapdev_init_stage_PHYSRC;//в следующем цикле вернусь
         return(FALSE);
       }
     //инициализация данного участка прошла успешно
     ENET_MIIWriteRegister(PhyAddr,PHY_BMCR,BMCR_RESET); // peqerhl PHY
     //готовлю переход к следующей стадии 
     tapdev_init_stage = tapdev_init_stage_PHY_TO;
     ProcStage = Lnk_Est_SetTo;//начать процесс
     return (FALSE);    
   }
  //////////////////////////////////////////////////////////////////////////// 
  if (tapdev_init_stage == tapdev_init_stage_PHY_TO) {
    switch (ProcStage){
      case Lnk_Est_SetTo:
        to = PHY_TO;
        ProcStage = Lnk_Est_Loop;//к следующей фазе
        return (FALSE);
      case Lnk_Est_Loop:
        if (to) {//ещё не дсчитал до нуля
          -- to;
          Reg = ENET_MIIReadRegister(PhyAddr,PHY_BMCR);
          if(!(Reg & BMCR_RESET))
            {//инициализация данного участка завершилась успешно, переход с след шагу
              tapdev_init_stage = tapdev_init_stage_PreambSuppression;
              return(FALSE);//в следующей итерации придёт куда нужно
            }          
          else {
            return (FALSE);//ещё не выполнилось условие цикла
          }
        }
        else {//досчитал до нуля, значит конекта нет,вернуться к начальному шагу
          tapdev_init_stage = tapdev_init_stage_HWINI;
          return(FALSE);
        }
        return (FALSE);//ещё не выполнилось условие цикла
    }
  }  
  ////////////////////////////////////////////////////////////////////////////
  if (tapdev_init_stage == tapdev_init_stage_PreambSuppression){
    if(ENET_MIIReadRegister(PhyAddr,PHY_BMSR ) & BMSR_NOPREAM)  // preamble suppression
     {
       // PHY support preamble suppression
       MCFG |= 1 << 1;
     }
    #if AUTO_NEGOTIATION_ENA != 0
    // PHY interrupt init, clear and enable
    // Set Auto-Negotiation Advertisement
    ENET_MIIWriteRegister(PhyAddr,PHY_ANAR, 1 | ANAR_10BT | ANAR_10BT_FULL | ANAR_100BT | ANAR_100BT_FULL);
    // Enable Auto-Negotiation
    Reg = BMCR_AN | BMCR_RE_AN;
    #else //AUTO_NEGOTIATION_ENA != 0
    // Auto-negotiation disabled
    Reg = 0;

    if(FIX_DUPLEX == FULL_DUPLEX){
       Reg |= BMCR_DUPLEX;
       IPGT = 0x15; // nopedeker bpel lefds o`jer`lh (0x15 pejnlemds~r dk full-duplex)
       MAC2_bit.FD = COMMAND_bit.FULLDUPLEX = 1;
     }

    if(FIX_SPEED == SPEED_100){
      Reg |= BMCR_SPEED_100;
      SUPP_bit.SPEED = 1;
    }
   #endif //AUTO_NEGOTIATION_ENA != 0
   ENET_MIIWriteRegister(PhyAddr,PHY_BMCR,Reg);
   ////////////////////////////////////////////
     tapdev_init_stage = tapdev_init_stage_LINK_ESTABLISHED;
     ProcStage = 0;//начать процесс
     return(FALSE);
  }

  //тут он висит долго, надо разбивать цикл
  ///////////////////////////////////////////////////////////////////////////
  if (tapdev_init_stage == tapdev_init_stage_LINK_ESTABLISHED) {
    switch (ProcStage){
      case Lnk_Est_SetTo:
        to = PHY_TO;
        ProcStage = Lnk_Est_Loop;//к следующей фазе
        return (FALSE);
      case Lnk_Est_Loop:
        if (to) {//ещё не дсчитал до нуля
          -- to;
          if(ENET_MIIReadRegister(PhyAddr,PHY_BMSR) & BMSR_LINK_ESTABLISHED) {
            //инициализация данного шага прошла успешно
            tapdev_init_stage = tapdev_init_stage_Run;//завершить настройку соединеия и работать
            return (FALSE);//в следующей итерации придёт куда нужно
          }
          else {
            return (FALSE);//ещё не выполнилось условие цикла
          }
        }
        else {//досчитал до нуля, значит конекта нет,вернуться к начальному шагу
          tapdev_init_stage = tapdev_init_stage_HWINI;
          return(FALSE);
        }
        return (FALSE);//ещё не выполнилось условие цикла
    }
  }
  ///////////////////////////////////////////////////////////////////////////
  if (tapdev_init_stage == tapdev_init_stage_Run) {
    for(Int32U i = 0; i < ENET_DMA_DESC_NUMB; ++i)
      {
        EnetDmaRx[i].EnetRxCtrl.Size = EMAC_MAX_PACKET_SIZE-1;
        EnetDmaRx[i].EnetRxCtrl.Intr = 1;
        EnetDmaRxSta[i].Data[0] = EnetDmaRxSta[i].Data[1] = 0;
        EnetDmaTx[i].EnetTxCtrl.Data = (1<<31) | (1<<30) | (1<<29) | (1<<28) | (1<<26) | (EMAC_MAX_PACKET_SIZE-1);
        EnetDmaTxSta[i].Data = 0;
       }
     EnetDmaRx[0].pBuffer = (pInt32U)RxBuff0;
     EnetDmaRx[1].pBuffer = (pInt32U)RxBuff1;
     EnetDmaTx[0].pBuffer = (pInt32U)TxBuff0;
     EnetDmaTx[1].pBuffer = (pInt32U)TxBuff1;
     RXCONSUMEINDEX = TXPRODUCEINDEX = 0;
     TXDESCRIPTOR = (Int32U)EnetDmaTx;
     TXSTATUS = (Int32U)EnetDmaTxSta;
     RXDESCRIPTOR = (Int32U)EnetDmaRx;
     RXSTATUS = (Int32U)EnetDmaRxSta;

     COMMAND_bit.RXENABLE = 1;
     MAC1_bit.RE = 1;
     COMMAND_bit.TXENABLE = 1;

     return(TRUE);
  }
  return(FALSE);
}

/*************************************************************************
 * Function Name: tapdev_read
 * Parameters:
 * Return:
 *
 * Description: Read data for MAC/DMA Controller
 *
 *************************************************************************/
Int32U tapdev_read(void * pPacket)
{
Int32U Indx = RXCONSUMEINDEX;
Int32U Size = EMAC_MAX_PACKET_SIZE;
  if(Indx == RXPRODUCEINDEX)  // }rn ngm`w`er, wrn mnbncn tpeil` mer
  {
    return(0);
  }
  Size = MIN(Size,(EnetDmaRxSta[Indx].RxSize+1)); // m` qksw`i, eqkh d`mm{u ank|xe,
                                                  // wel l`jqhl`k|mn dnosqrhl{i p`glep o`jer`?
  // jnohpsel d`mm{e p`glepnl Size hg EnetDmaRx[Indx].pBuffer
  // b nak`qr| o`lrh, m` jnrnps~ sj`g{b`er pPacket
  memcpy(pPacket,EnetDmaRx[Indx].pBuffer,Size);
  if(++Indx > RXDESCRIPTORNUMBER)
  {
    Indx = 0;
  }
  RXCONSUMEINDEX = Indx;
  return(Size);
}

/*************************************************************************
 * Function Name: tapdev_send
 * Parameters:
 * Return: Boolean
 *
 * Description: Send data to MAC/DMA Controller
 *
 *************************************************************************/
Boolean tapdev_send(void *pPacket, Int32U size)
{
Int32U Indx, IndxHold = TXPRODUCEINDEX + 1;
  if(size == 0)
  {
    return(TRUE);
  }
  if(IndxHold > TXDESCRIPTORNUMBER)
  {
    IndxHold = 0;
  }
  if(IndxHold == TXCONSUMEINDEX)
  {
    return(FALSE);
  }
  Indx = TXPRODUCEINDEX;
  size = MIN(size,EMAC_MAX_PACKET_SIZE);
  memcpy(EnetDmaTx[Indx].pBuffer,pPacket,size);
  EnetDmaTx[Indx].EnetTxCtrl.Size = size - 1;
  TXPRODUCEINDEX = IndxHold;

  return(TRUE);
}

/*************************************************************************
 * Function Name: ENET_MIIWriteRegister
 * Parameters: Int8U DevId, Int8U RegAddr, Int32U Value
 * Return: none
 *
 * Description: Writes a value on the PHY registers
 *
 *************************************************************************/
static void ENET_MIIWriteRegister (Int8U DevId, Int8U RegAddr, Int32U Value)
{
  MCMD = 0;                   // set read operation
  MADR_bit.PHY_ADDR = DevId;  // set the MII Physical address
  MADR_bit.REGADDR = RegAddr; // set the MII register address
  MWTD = Value;
  while(MIND_bit.BUSY);
}

/*************************************************************************
 * Function Name: ENET_MIIReadRegister
 * Parameters: Int8U DevId, Int8U RegAddr, Int32U Value
 * Return: Int32U
 *
 * Description: Read a value from the PHY registers
 *
 *************************************************************************/
static Int32U ENET_MIIReadRegister (Int8U DevId, Int8U RegAddr)
{
  MCMD = 0;
  MADR_bit.PHY_ADDR = DevId;  // set the MII Physical address
  MADR_bit.REGADDR = RegAddr; // set the MII register address
  MCMD = 1;                   // set read operation
  while(MIND_bit.BUSY | MIND_bit.NOT_VALID);
  return(MRDD);
}


