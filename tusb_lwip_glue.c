/* 
 * The MIT License (MIT)
 *
 * Based on tinyUSB example that is: Copyright (c) 2020 Peter Lawrence
 * Modified for Pico by Floris Bos
 *
 * influenced by lrndis https://github.com/fetisov/lrndis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "tusb_lwip_glue.h"
#include "pico/unique_id.h"
#include "lwip/apps/httpd.h"
//#include "lwip/apps/fs.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwipopts.h"
#include "hardware/adc.h"

/* lwip context */
static struct netif netif_data;

/* shared between tud_network_recv_cb() and service_traffic() */
static struct pbuf *received_frame;

/* this is used by this code, ./class/net/net_driver.c, and usb_descriptors.c */
/* ideally speaking, this should be generated from the hardware's unique ID (if available) */
/* it is suggested that the first byte is 0x02 to indicate a link-local address */
const uint8_t tud_network_mac_address[6] = {0x02,0x02,0x84,0x6A,0x96,0x00};

/* network parameters of this MCU */
static const ip_addr_t ipaddr  = IPADDR4_INIT_BYTES(192, 168, 7, 1);
static const ip_addr_t netmask = IPADDR4_INIT_BYTES(255, 255, 255, 0);
static const ip_addr_t gateway = IPADDR4_INIT_BYTES(0, 0, 0, 0);

/* database IP addresses that can be offered to the host; this must be in RAM to store assigned MAC addresses */
static dhcp_entry_t entries[] =
{
    /* mac ip address                          lease time */
    { {0}, IPADDR4_INIT_BYTES(192, 168, 7, 2), 24 * 60 * 60 },
    { {0}, IPADDR4_INIT_BYTES(192, 168, 7, 3), 24 * 60 * 60 },
    { {0}, IPADDR4_INIT_BYTES(192, 168, 7, 4), 24 * 60 * 60 },
};

static const dhcp_config_t dhcp_config =
{
    .router = IPADDR4_INIT_BYTES(0, 0, 0, 0),  /* router address (if any) */
    .port = 67,                                /* listen port */
    .dns = IPADDR4_INIT_BYTES(0, 0, 0, 0),     /* dns server (if any) */
    "",                                        /* dns suffix */
    TU_ARRAY_SIZE(entries),                    /* num entry */
    entries                                    /* entries */
};

// max length of the tags defaults to be 8 chars
// LWIP_HTTPD_MAX_TAG_NAME_LEN
const char * __not_in_flash("httpd") ssi_example_tags[] = {
    "Hello",    // 0
    "counter",  // 1
    "GPIO",     // 2
    "state1",   // 3
    "state2",   // 4
    "state3",   // 5
    "state4",   // 6
    "bg1",      // 7
    "bg2",      // 8
    "bg3",      // 9
    "bg4"       // 10
};

static err_t linkoutput_fn(struct netif *netif, struct pbuf *p)
{
    (void)netif;
    
    for (;;)
    {
      /* if TinyUSB isn't ready, we must signal back to lwip that there is nothing we can do */
      if (!tud_ready())
        return ERR_USE;
    
      /* if the network driver can accept another packet, we make it happen */
      if (tud_network_can_xmit(p->tot_len))
      {
        tud_network_xmit(p, 0 /* unused for this example */);
        return ERR_OK;
      }
    
      /* transfer execution to TinyUSB in the hopes that it will finish transmitting the prior packet */
      tud_task();
    }
}

static err_t output_fn(struct netif *netif, struct pbuf *p, const ip_addr_t *addr)
{
    return etharp_output(netif, p, addr);
}

static err_t netif_init_cb(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));
    netif->mtu = CFG_TUD_NET_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
    netif->state = NULL;
    netif->name[0] = 'E';
    netif->name[1] = 'X';
    netif->linkoutput = linkoutput_fn;
    netif->output = output_fn;
    return ERR_OK;
}

void init_lwip(void)
{
    struct netif *netif = &netif_data;
    
    /* Fixup MAC address based on flash serial */
    //pico_unique_board_id_t id;
    //pico_get_unique_board_id(&id);
    //memcpy( (tud_network_mac_address)+1, id.id, 5);
    // Fixing up does not work because tud_network_mac_address is const
    
    /* Initialize tinyUSB */
    tusb_init();
    
    /* Initialize lwip */
    lwip_init();
    
    /* the lwip virtual MAC address must be different from the host's; to ensure this, we toggle the LSbit */
    netif->hwaddr_len = sizeof(tud_network_mac_address);
    memcpy(netif->hwaddr, tud_network_mac_address, sizeof(tud_network_mac_address));
    netif->hwaddr[5] ^= 0x01;
    
    netif = netif_add(netif, &ipaddr, &netmask, &gateway, NULL, netif_init_cb, ip_input);
    netif_set_default(netif);
}

void tud_network_init_cb(void)
{
    /* if the network is re-initializing and we have a leftover packet, we must do a cleanup */
    if (received_frame)
    {
      pbuf_free(received_frame);
      received_frame = NULL;
    }
}

bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
    /* this shouldn't happen, but if we get another packet before 
    parsing the previous, we must signal our inability to accept it */
    if (received_frame) return false;
    
    if (size)
    {
        struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);

        if (p)
        {
            /* pbuf_alloc() has already initialized struct; all we need to do is copy the data */
            memcpy(p->payload, src, size);
        
            /* store away the pointer for service_traffic() to later handle */
            received_frame = p;
        }
    }

    return true;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
    struct pbuf *p = (struct pbuf *)ref;
    struct pbuf *q;
    uint16_t len = 0;

    (void)arg; /* unused for this example */

    /* traverse the "pbuf chain"; see ./lwip/src/core/pbuf.c for more info */
    for(q = p; q != NULL; q = q->next)
    {
        memcpy(dst, (char *)q->payload, q->len);
        dst += q->len;
        len += q->len;
        if (q->len == q->tot_len) break;
    }

    return len;
}

void service_traffic(void)
{
    /* handle any packet received by tud_network_recv_cb() */
    if (received_frame)
    {
      ethernet_input(received_frame, &netif_data);
      pbuf_free(received_frame);
      received_frame = NULL;
      tud_network_recv_renew();
    }
    
    sys_check_timeouts();
}

void dhcpd_init()
{
    while (dhserv_init(&dhcp_config) != ERR_OK);    
}

void wait_for_netif_is_up()
{
    while (!netif_is_up(&netif_data));    
}


/* lwip platform specific routines for Pico */
auto_init_mutex(lwip_mutex);
static int lwip_mutex_count = 0;

sys_prot_t sys_arch_protect(void)
{
    uint32_t owner;
    if (!mutex_try_enter(&lwip_mutex, &owner))
    {
        if (owner != get_core_num())
        {
            // Wait until other core releases mutex
            mutex_enter_blocking(&lwip_mutex);
        }
    }

    lwip_mutex_count++;
    
    return 0;
}

void sys_arch_unprotect(sys_prot_t pval)
{
    (void)pval;
    
    if (lwip_mutex_count)
    {
        lwip_mutex_count--;
        if (!lwip_mutex_count)
        {
            mutex_exit(&lwip_mutex);
        }
    }
}

uint32_t sys_now(void)
{
    return to_ms_since_boot( get_absolute_time() );
}



u16_t __time_critical_func(ssi_handler)(int iIndex, char *pcInsert, int iInsertLen)
{
    size_t printed;
    switch (iIndex) {
        case 0: /* "Hello" */
            printed = snprintf(pcInsert, iInsertLen, "Hello user number %d!", rand());
            break;
        case 1: /* "counter" */
        {
            static int counter;
            counter++;
            printed = snprintf(pcInsert, iInsertLen, "%d", counter);
        }
            break;
        case 2: /* "GPIO" */
        {
            const float voltage = adc_read() * 3.3f / (1 << 12);
            printed = snprintf(pcInsert, iInsertLen, "%f", voltage);
        }
            break;
        case 3: /* "state1" */
        case 4: /* "state2" */
        case 5: /* "state3" */
        case 6: /* "state4" */
        {
            bool state;
            if(iIndex == 3)
                state = gpio_get(LED1);
            else if(iIndex == 4)
                state = gpio_get(LED2);
            else if(iIndex == 5)
                state = gpio_get(LED3);
            else if(iIndex == 6)
                state = gpio_get(LED4);

            if(state)
                printed = snprintf(pcInsert, iInsertLen, "checked");
            else
                printed = snprintf(pcInsert, iInsertLen, " ");
        }
          break;

        case 7:  /* "bg1" */
        case 8:  /* "bg2" */
        case 9:  /* "bg3" */
        case 10: /* "bg4" */
        {
            bool state;
            if(iIndex == 7)
                state = gpio_get(LED1);
            else if(iIndex == 8)
                state = gpio_get(LED2);
            else if(iIndex == 9)
                state = gpio_get(LED3);
            else if(iIndex == 10)
                state = gpio_get(LED4);

            if(state)
                printed = snprintf(pcInsert, iInsertLen, "\"background-color:green;\"");
            else
                printed = snprintf(pcInsert, iInsertLen, "\"background-color:red;\"");
        }
          break;
        default: /* unknown tag */
            printed = 0;
            break;
    }
      LWIP_ASSERT("sane length", printed <= 0xFFFF);
      return (u16_t)printed;
}

void ssi_init()
{
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0);
    size_t i;
    for (i = 0; i < LWIP_ARRAYSIZE(ssi_example_tags); i++) {
        LWIP_ASSERT("tag too long for LWIP_HTTPD_MAX_TAG_NAME_LEN",
                    strlen(ssi_example_tags[i]) <= LWIP_HTTPD_MAX_TAG_NAME_LEN);
    }

      http_set_ssi_handler(ssi_handler,
                           ssi_example_tags, LWIP_ARRAYSIZE(ssi_example_tags)
      );
}

static const tCGI cgi_handlers[] = {
    {
        /* Html request for "/leds.cgi" will start cgi_handler_basic */
        "/leds.cgi", cgi_handler_basic
    },
    {
        /* Html request for "/leds2.cgi" will start cgi_handler_extended */
        "/leds_ext.cgi", cgi_handler_extended
    },
    {
        "/test.cgi", cgi_handler_post
    }
};



/* cgi-handler triggered by a request for "/leds.cgi" */
const char *
cgi_handler_basic(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i=0;

    /* We use this handler for one page request only: "/leds.cgi"
     * and it is at position 0 in the tCGI array (see above).
     * So iIndex should be 0.
     */
    printf("cgi_handler_basic called with index %d\n", iIndex);

    /* All leds off */
    Led_Off(LED1);
    Led_Off(LED2);
    Led_Off(LED3);
    Led_Off(LED4);

    /* Check the query string.
     * A request to turn LED2 and LED4 on would look like: "/leds.cgi?led=2&led=4"
     */
    for (i = 0; i < iNumParams; i++){
        /* check if parameter is "led" */
        if (strcmp(pcParam[i] , "led") == 0){
            /* look ar argument to find which led to turn on */
            if(strcmp(pcValue[i], "1") == 0)
                Led_On(LED1);
            else if(strcmp(pcValue[i], "2") == 0)
                Led_On(LED2);
            else if(strcmp(pcValue[i], "3") == 0)
                Led_On(LED3);
            else if(strcmp(pcValue[i], "4") == 0)
                Led_On(LED4);
        }
    }

    /* Our response to the "SUBMIT" is to simply send the same page again*/
    return "/cgi.html";
}

/* cgi-handler triggered by a request for "/leds_ext.cgi".
 *
 * It is almost identical to cgi_handler_basic().
 * Both handlers could be easily implemented in one function -
 * distinguish them by looking at the iIndex parameter.
 * I left it this way to show how to implement two (or more)
 * enirely different handlers.
 */
const char *
cgi_handler_extended(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    int i=0;

    /* We use this handler for one page request only: "/leds_ext.cgi"
     * and it is at position 1 in the tCGI array (see above).
     * So iIndex should be 1.
     */
    printf("cgi_handler_extended called with index %d\n", iIndex);

    /* All leds off */
    Led_Off(LED1);
    Led_Off(LED2);
    Led_Off(LED3);
    Led_Off(LED4);

    /* Check the query string.
     * A request to turn LED2 and LED4 on would look like: "/leds.cgi?led=2&led=4"
     */
    for (i = 0; i < iNumParams; i++){
        /* check if parameter is "led" */
        if (strcmp(pcParam[i] , "led") == 0){
            /* look ar argument to find which led to turn on */
            if(strcmp(pcValue[i], "1") == 0)
                Led_On(LED1);
            else if(strcmp(pcValue[i], "2") == 0)
                Led_On(LED2);
            else if(strcmp(pcValue[i], "3") == 0)
                Led_On(LED3);
            else if(strcmp(pcValue[i], "4") == 0)
                Led_On(LED4);
        }
    }

    /* Our response to the "SUBMIT" is to send "/ssi_cgi.shtml".
     * The extension ".shtml" tells the server to insert some values
     * which show the user what has been done in response.
     */
    return "/ssi_cgi.shtml";
}

const char * cgi_handler_post(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    printf("cgi handler for POST");
    return "/index.html";
}

/* initialize the CGI handler */
void
cgi_init(void)
{
    http_set_cgi_handlers(cgi_handlers, LWIP_ARRAYSIZE(cgi_handlers));

    for(int i = LED1; i <= LED4; i++){
        gpio_init(i);
        gpio_set_dir(i, GPIO_OUT);
        gpio_put(i, 0);
    }
}

/* led control and debugging info */
void
Led_On(int led)
{
    printf("GPIO%d on\n", led);
    gpio_put(led, 1);
}

void
Led_Off(int led)
{
    printf("GPIO%d off\n", led);
    gpio_put(led, 0);
}

//open custom file

// int fs_open_custom(struct fs_file *file, const char *name) 
// {
// //     const char generated_html[] =
// // "<html>\n"
// // "<head><title>lwIP - A Lightweight TCP/IP Stack</title></head>\n"
// // " <body bgcolor=\"white\" text=\"black\">\n"
// // "  <table width=\"100%\">\n"
// // "   <tr valign=\"top\">\n"
// // "    <td width=\"80\">\n"
// // "     <a href=\"http://www.sics.se/\"><img src=\"/img/sics.gif\"\n"
// // "      border=\"0\" alt=\"SICS logo\" title=\"SICS logo\"></a>\n"
// // "    </td>\n"
// // "    <td width=\"500\">\n"
// // "     <h1>lwIP - A Lightweight TCP/IP Stack</h1>\n"
// // "     <h2>Generated page</h2>\n"
// // "     <p>This page might be generated in-memory at runtime</p>\n"
// // "    </td>\n"
// // "    <td>\n"
// // "    &nbsp;\n"
// // "    </td>\n"
// // "   </tr>\n"
// // "  </table>\n"
// // " </body>\n"
// // "</html>";
// //     /* this example only provides one file */
// //     if (!strcmp(name, "/generated.html")) {
// //         /* initialize fs_file correctly */
// //         memset(file, 0, sizeof(struct fs_file));
// //         file->pextension = mem_malloc(sizeof(generated_html));
// //         if (file->pextension != NULL) {
// //             /* instead of doing memcpy, you would generate e.g. a JSON here */
// //             memcpy(file->pextension, generated_html, sizeof(generated_html));
// //             file->data = (const char *)file->pextension;
// //             file->len = sizeof(generated_html) - 1; /* don't send the trailing 0 */
// //             file->index = file->len;
// //             /* allow persisteng connections */
// //             file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
// //             return 1;
// //         }
// //     }
// //     if (!strcmp(name, "/testing.txt")) {
// //         memset(file, 0, sizeof(struct fs_file));
// //         file->data = text_entry;
// //         file->len = sizeof(text_entry) - 1;
// //         file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
// //         return 1;
// //     }

//     return 0; 
// }

// // //read file and fill “buffer” step by step by “count” bytes amount
// int fs_read_custom(struct fs_file *file, char *buffer, int count) {

//     int read = 0;


//         //end reached
//     read = FS_READ_EOF;
//     return read;
// }

// void fs_close_custom(struct fs_file *file) {

//   if (file && file->pextension) {
//     mem_free(file->pextension);
//     file->pextension = NULL;
//   }

// }