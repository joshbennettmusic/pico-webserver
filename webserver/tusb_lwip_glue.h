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
#define DSP_RST 6

void init_lwip();
void wait_for_netif_is_up();
void dhcpd_init();
void service_traffic();

u16_t __time_critical_func(ssi_handler)(int iIndex, char *pcInsert, int iInsertLen);
void ssi_init();
/* initialize the CGI handler */
void  cgi_init(void);

#ifdef __cplusplus
 }
#endif

#endif