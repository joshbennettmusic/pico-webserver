#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include <string.h>
#include "pico/bootrom.h"
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"
#include "W25Q64.h"

#include "tusb_lwip_glue.h"


#define LED_PIN     25
// FLASH SPI
#define FLASH_PORT spi0
// SPI SELECT FLASH OR DISPLAY
#define SEL_FLASHB_DISP     28
#define FLASH_RX  16
#define FLASH_CS  17
#define FLASH_SCK 18
#define FLASH_TX  19


// let our webserver do some dynamic handling
// static const char *cgi_toggle_led(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
// {
//     gpio_put(LED_PIN, !gpio_get(LED_PIN));
//     return "/index.html";
// }

// static const char *cgi_reset_usb_boot(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
// {
//     reset_usb_boot(0, 0);
//     return "/index.html";
// }

// static const tCGI cgi_handlers[] = {
//   {
//     "/toggle_led",
//     cgi_toggle_led
//   },
//   {
//     "/reset_usb_boot",
//     cgi_reset_usb_boot
//   }
// };
// Flash B
FlashMemory flashB(FLASH_PORT, FLASH_RX, FLASH_TX, FLASH_CS, FLASH_SCK, SEL_FLASHB_DISP);


int main()
{
    // initialise IO and devices
    stdio_init_all();
    
    flashB.init();
    // Initialize tinyusb, lwip, dhcpd and httpd
    init_lwip();
    wait_for_netif_is_up();
    dhcpd_init();
    httpd_init();
    ssi_init();
    cgi_init();
    //http_set_cgi_handlers(cgi_handlers, LWIP_ARRAYSIZE(cgi_handlers));
    
    // For toggle_led
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    while (true)
    {
        tud_task();
        service_traffic();
    }

    return 0;
}
