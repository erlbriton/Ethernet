//настройки и работа TCP/IP
#include "tcp_ctrl.h"
#include "bastypes.h"
#include "ws_uart_bridge.h"


void tcp_reinit (void);//перезапуск например при изменении IP адреса или 
Boolean tcp_ctrl_init (void);//фоновая инициализация
void tcp_ctrl_update (void);//работа стека TCP/IP  

  static uip_ipaddr_t ipaddr;
  static u16_t ipaddr0, ipaddr1, ipaddr2, ipaddr3;
  struct timer periodic_timer, arp_timer;
  static baulong ip;
  
  static Boolean tcp_ready = FALSE; //признак что связь по Ethernet не активирована
  
#define BUF ((struct uip_eth_hdr *)&uip_buf[0]) // указатель на структуру uip_eth_hdr (заголовок Ethernet)
                                                // с адресом uip_buf[0]
/* Массив uip_buf используется для хранения входящих и исходящих пакетов.
 * Драйвер должен складывать входящие данные в этот буфер.
 * При отправке данных драйвер читает link level-заголовки и TCP/IP заголовки
 * из данного буфера. Размер link level-заголовков содержится в UIP_LLH_LEN
 *
 * Данные при отправке в этот буфер не складываются.
 * Драйвер должен читать их из ячеек, на которые указывает указатель uip_appdata
*/
  
void tcp_reinit (void){//перезапуск например при изменении IP адреса или 
 // if (RAM_DATA.IP_ADDR != RAM_DATA.mIP_ADDR) {
    //если не совпалают IP то init выполняется заново.
    tapdev_init_stage = tapdev_init_stage_HWINI ;//стадия инициализации езернета
    tcp_ready  = FALSE;
  //}
}

Boolean tcp_ctrl_init (void){
  //Ethernet init

    tcp_ready = tapdev_init();
    if (!tcp_ready) return (FALSE);
//      printf("TCP INIT OK");
   
    // Sys timer init 1/200 sec tick
      timer_set(&periodic_timer, CLOCK_SECOND / (WAIT_RESPOND*1));//Таймер0 тут 100 тиков в сек / 40 = 2 тика или 0,02 сек
                                                                  //RTC 1000 тиков в сек / 40 = 20 что тоже 0,02 сек
      timer_set(&arp_timer, CLOCK_SECOND * 10); //100 тиков в сек * 10 = 1000 тиков или 10 сек
                                                //RTC 1000 тиков в сек * 10 = 10000 что тоже 10 сек
      clock_init(3);//запуск Timer 0
      // uIP web server
    // Initialize the uIP TCP/IP stack.
      uip_init();
    //  uip_ipaddr(ipaddr, 192,168,0,198);
    // Конфигурируем адрес карты (его берем из Flash регистра IP_ADDR)
      ip.i = RAM_DATA.IP_ADDR;
      ipaddr0 = ip.b[3];//192;//ip.b[3];//F020_FLASH_Reg.F020[145];
      ipaddr1 = ip.b[2];//168;//ip.b[2];//F020_FLASH_Reg.F020[144];
      ipaddr2 = ip.b[1];//0;//88;//88;//ip.b[1];//F020_FLASH_Reg.F020[143];
      ipaddr3 = ip.b[0];//100;//77;//ip.b[0];//F020_FLASH_Reg.F020[142]; 
      RAM_DATA.IP_ADDR = ip.i; //показать полученный IP 
   // вычислить и установить MAC-адрес сетевой карты
   #if UIP_FIXEDETHADDR == 0
      //подготовка МАС
      struct uip_eth_addr mac_addr;
      mac_addr.addr [0] = 0x00;     // Micrel's
      mac_addr.addr [1] = 0x10;     // OUI is
      mac_addr.addr [2] = 0xA1;     // 0x0010A1
      mac_addr.addr [3] = ~ipaddr1;
      mac_addr.addr [4] = ~ipaddr2;
      mac_addr.addr [5] = ~ipaddr3;
      //установка МАС
      uip_setethaddr (mac_addr);  
   #endif /* UIP_FIXEDETHADDR == 0 */  
   //установка IP
     uip_ipaddr(ipaddr, ipaddr0, ipaddr1, ipaddr2, ipaddr3);
     uip_sethostaddr(ipaddr);
   // default router's IP address
     //uip_ipaddr(ipaddr, 192,168,88,1);  
     uip_ipaddr(ipaddr, 192,168,0,1);  
     uip_setdraddr(ipaddr);
     //
     uip_ipaddr(ipaddr, 255,255,255,0);
     uip_setnetmask(ipaddr); 
     //     
     uip_gethostaddr(ipaddr);
   //
   // Initialize the TCP to UART bridge.
     tcpip_uart_bridge_init();     
	 // Initialize the WebSocket to UART bridge (port 8080).
    ws_uart_bridge_init();
   ///////////////////////////////////
     return (TRUE);
}

void tcp_ctrl_update (void){
  u32 i;//
  //Инициализация если ещё не инициализирован
  if (!tcp_ready) {
    //printf("TCP INIT DETECTED");
    tcp_ctrl_init();
    return;
  }
     

  //работа TCP/IP
  //проверяем, пришел ли пакет из сети
  uip_len = tapdev_read(uip_buf); // Read data for MAC/DMA Controller
//  printf("Uip_len = %d\r\n",uip_len);
  if(uip_len > 0) {// значит в буфере лежит пакет!
      //если тип заголовка пакета UIP_ETHTYPE_IP - это IPv4
      //входящий IP пакет  
      if(BUF->type == htons(UIP_ETHTYPE_IP))
      {  
     //  printf("IP PACKET DETECTED \r\n");
        FIO0CLR  =  ETHERNET_LED; // зажгли LINK3 если пришел IP-пакет

        uip_arp_ipin(); // ARP обработка входящих IP пакетов (проверяет, есть ли этот IP в кэше...)
                        // собсна обработка входящего пакета (пакет в uip_buff[])
	      uip_input();  // uip_process(UIP_DATA)
	      // If the above function invocation resulted in data that
	      // should be sent out on the network, the global variable
	      // uip_len is set to a value > 0.
	      if(uip_len > 0) // в этом случае глобальная переменная uip_len содержит длину пакета,
        {                     // ктр нужно отослать (пакет уже лежит в uip_buf[])
	        uip_arp_out();  // присоединяет Ethernet-заголовок отправляемому IP пакету и ...
	        tapdev_send(uip_buf,uip_len); // Send data to MAC/DMA Controller
	      }
        FIO0SET  =  ETHERNET_LED;  // гасим LINK3 независимо от того, отправляли мы ответ или нет
      }
      // если тип заголовка пакета UIP_ETHTYPE_ARP - это
      // входящий ARP пакет
      else if(BUF->type == htons(UIP_ETHTYPE_ARP))
      {
   //     printf("ARP PACKET DETECTED \r\n");
        uip_arp_arpin(); // ARP обработка входящих ARP пакетов
	      // If the above function invocation resulted in data that
	      // should be sent out on the network, the global variable
	      // uip_len is set to a value > 0.
	      if(uip_len > 0)  // в этом случае глобальная переменная uip_len содержит длину пакета,
              {                // ктр нужно отослать (пакет лежит в uip_buf[])
	        tapdev_send(uip_buf,uip_len); // Send data to MAC/DMA Controller
	      }
      }
    }
    // проверяем периодический таймаут    
//    else if(timer_expired(&periodic_timer))
//    {
//      printf("Timeout now\r\n");
//      timer_reset(&periodic_timer);
//      for(i = 0; i < UIP_CONNS; i++)
//      {
//      	uip_periodic(i); // uip_process (UIP_TIMER) периодически опрашиваем соединение i
//        //If the above function invocation resulted in data that
//        //should be sent out on the network, the global variable
//        //uip_len is set to a value > 0.
//        if(uip_len > 0)
//        {
//          //FIO0CLR  =  ETHERNET_LED; // зажгли LINK3 если требуется отправка после опроса соединений
//          uip_arp_out();  // присоединяет Ethernet-заголовок отправляемому IP пакету и ...
//          tapdev_send(uip_buf,uip_len); // Send data to MAC/DMA Controller
//          //FIO0SET  =  ETHERNET_LED; // погасили LINK3 после отправки
//        }
//      }
//#if UIP_UDP
//
//      for(i = 0; i < UIP_UDP_CONNS; i++) {
//        uip_udp_periodic(i);
//        // If the above function invocation resulted in data that
//        // should be sent out on the network, the global variable
//        // uip_len is set to a value > 0.
//        if(uip_len > 0) {
//          uip_arp_out();
//          tapdev_send();
//        }
//      }
//#endif // UIP_UDP 
//      // Call the ARP timer function every 10 seconds.
//      if(timer_expired(&arp_timer))
//      {
////FIO0PIN ^= WRK_LED;
//        timer_reset(&arp_timer);
//        uip_arp_timer();
//      }
//    }
}


