#include "nrf51.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "w5500.h"
#include "wizchip_conf.h"
#include "socket.h"
#include "hal.h"
#include <string.h>
/* Private macro -------------------------------------------------------------*/
uint8_t gDATABUF[2048];//????????:2048

uint8_t memsize[2][8] = {{2,2,2,2,2,2,2,2},{2,2,2,2,2,2,2,2}}; /* WIZCHIP SOCKET Buffer initialize */
wiz_NetInfo gWIZNETINFO = { .mac = {0x00, 0x08, 0xdc,0x00, 0xab, 0xcd},//MAC??
                            .ip = {192, 168, 3, 127},                  //IP??
                            .sn = {255,255,255,0},                     //????
                            .gw = {192, 168, 3, 1},                    //????
                            .dns = {114,114,114,114},                   //DNS???
                            .dhcp = NETINFO_DHCP  // NETINFO_STATIC
};


/* Private functions ---------------------------------------------------------*/
static void RegisterSPItoW5500(void);/*?SPI???????W5500?socket??*/
static void InitW5500SocketBuf(void);/*???W5500????*/
static void PhyLinkStatusCheck(void);/* PHY??????*/
static void DhcpInitHandler(void);   /*DHCP???*/
static void my_ip_assign(void);      /*????IP*/          
static void my_ip_conflict(void);    /*IP???????????*/
void network_init(void);      /*?????*/
void DNS_Analyse(void);/*DNS??*/



/*-------------------------------------------------*/
/*???:?????                               */
/*?  ?:?                                       */
/*???:?                                       */
/*-------------------------------------------------*/
void SPI_CrisEnter(void)
{
	__set_PRIMASK(1);    //??????
}
/*-------------------------------------------------*/
/*???:?????                               */
/*?  ?:?                                       */
/*???:?                                       */
/*-------------------------------------------------*/
void SPI_CrisExit(void)
{
	__set_PRIMASK(0);   //?????
}

/*-------------------------------------------------*/
/*???: ?????????                      */
/*?  ?:?                                       */
/*???:?                                       */
/*-------------------------------------------------*/
void SPI_CS_Select(void)
{
	nrf_gpio_pin_clear(W5500_SPI_CS_PIN);
}

/*-------------------------------------------------*/
/*???: ?????????                      */
/*?  ?:?                                       */
/*???:?                                       */
/*-------------------------------------------------*/
void SPI_CS_Deselect(void)
{
	nrf_gpio_pin_set(W5500_SPI_CS_PIN); 
}


/* IP??????????? */
static void my_ip_conflict(void)
{
    UART_Printf("CONFLICT IP from DHCP\r\n");
    
    //halt or reset or any...
    while(1); // this example is halt.
}

/*-------------------------------------------------*/
/*???: ?????????                      */
/*?  ?:?                                       */
/*???:?                                       */
/*-------------------------------------------------*/
void W5500_Reset(void)
{
	nrf_gpio_pin_clear(W5500_RST_PIN);
	delay_ms(500);
	nrf_gpio_pin_set(W5500_RST_PIN);
	delay_ms(2000);
}

/*******************************************************
 * @ brief Call back for ip assing & ip update from DHCP
 * ????IP???
 *******************************************************/
static void my_ip_assign(void)
{
   getIPfromDHCP(gWIZNETINFO.ip);  //IP??
   getGWfromDHCP(gWIZNETINFO.gw);  //????
   getSNfromDHCP(gWIZNETINFO.sn);  //????
   getDNSfromDHCP(gWIZNETINFO.dns);//DNS???
   gWIZNETINFO.dhcp = NETINFO_DHCP;
   
   /* Network initialization */
   network_init();//??DHCP??????????????
   UART_Printf("DHCP LEASED TIME : %d Sec.\r\n", getDHCPLeasetime());//?????DHCP??????

   DNS_Analyse();//????
}


/******************************************************************************
 * @brief  Network Init
 * Intialize the network information to be used in WIZCHIP
 *****************************************************************************/
void network_init(void)
{
    uint8_t tmpstr[6] = {0};
    wiz_NetInfo netinfo;

    /*??gWIZNETINFO?????????*/
    //DNS???gWIZNETINFO?????????,?????????????
    ctlnetwork(CN_SET_NETINFO, (void*)&gWIZNETINFO);

    /*????????????netinfo???*/
    ctlnetwork(CN_GET_NETINFO, (void*)&netinfo);

    /* ???????? */
    ctlwizchip(CW_GET_ID,(void*)tmpstr);
    if(netinfo.dhcp == NETINFO_DHCP) 
      UART_Printf("\r\n=== %s NET CONF : DHCP ===\r\n",(char*)tmpstr);
    else 
      UART_Printf("\r\n=== %s NET CONF : Static ===\r\n",(char*)tmpstr);

    UART_Printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",netinfo.mac[0],netinfo.mac[1],netinfo.mac[2],
                    netinfo.mac[3],netinfo.mac[4],netinfo.mac[5]);
    UART_Printf("SIP: %d.%d.%d.%d\r\n", netinfo.ip[0],netinfo.ip[1],netinfo.ip[2],netinfo.ip[3]);
    UART_Printf("GAR: %d.%d.%d.%d\r\n", netinfo.gw[0],netinfo.gw[1],netinfo.gw[2],netinfo.gw[3]);
    UART_Printf("SUB: %d.%d.%d.%d\r\n", netinfo.sn[0],netinfo.sn[1],netinfo.sn[2],netinfo.sn[3]);
    UART_Printf("DNS: %d.%d.%d.%d\r\n", netinfo.dns[0],netinfo.dns[1],netinfo.dns[2],netinfo.dns[3]);
    UART_Printf("===========================\r\n");
}
 
static void RegisterSPItoW5500(void)
{
  
  reg_wizchip_cris_cbfunc(SPI_CrisEnter, SPI_CrisExit);
  
  /* 2.??SPI?????? */

  reg_wizchip_cs_cbfunc(SPI_CS_Select, SPI_CS_Deselect);
      
  /* 3.?????? */
  reg_wizchip_spi_cbfunc(SPI_ReadByte, SPI_WriteByte);
}

/*???W5500????:?????????????*/
static void InitW5500SocketBuf(void)
{
  /* WIZCHIP SOCKET Buffer initialize */
  if(ctlwizchip(CW_INIT_WIZCHIP,(void*)memsize) == -1){
      UART_Printf("WIZCHIP Initialized fail.\r\n");
//      while(1);
	  NVIC_SystemReset();                                //??
  }
}
 
static void PhyLinkStatusCheck(void)
{
	uint8_t tmp;
	uint8_t Count_Over = 0;
	__disable_interrupt();

	do{
		if(ctlwizchip(CW_GET_PHYLINK, (void*)&tmp) == -1){
			UART_Printf("Unknown PHY Link stauts.\r\n");
		}
		if(tmp == PHY_LINK_OFF){
			UART_Printf("PHY Link OFF!!!\r\n");//?????,?????,??????
			nrf_delay_ms(2000);      
			break;
			} 
		 
	}while(tmp == PHY_LINK_OFF);

	__enable_interrupt();
 
		UART_Printf(" PHY Link Success.\r\n");
 
}

 
int main(void)
{  
    while(1) 
    {  
				nrf_gpio_pin_clear(21);
				nrf_delay_ms(100);
				nrf_gpio_pin_set(21);
        nrf_delay_ms(100); 
    }     
}
