#ifndef _TUSB_LWIP_GLUE_H_
#define _TUSB_LWIP_GLUE_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include <string.h>
#include <stdlib.h>

#include "tusb.h"
#include "dhserver.h"
#include "dnserver.h"
#include "lwip/init.h"
#include "lwip/timeouts.h"
#include "lwip/apps/httpd.h"

// GPIOs for Leds
#define LED1	12
#define LED2	13
#define LED3	14
#define LED4	15

void init_lwip();
void wait_for_netif_is_up();
void dhcpd_init();
void service_traffic();

u16_t __time_critical_func(ssi_handler)(int iIndex, char *pcInsert, int iInsertLen);
void ssi_init();
/* initialize the CGI handler */
void  cgi_init(void);

/* CGI handler for LED control */
const char * cgi_handler_basic(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);
/* CGI handler for LED control with feedback*/
const char * cgi_handler_extended(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);
// CG Handler for POST
const char * cgi_handler_post(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);

/* led control and debugging info */
void Led_On(int led);
void Led_Off(int led);


#ifdef __cplusplus
 }
#endif

#endif