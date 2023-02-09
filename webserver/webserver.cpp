#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include <string.h>
#include "pico/bootrom.h"
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"
#include "hardware/adc.h"

#include "webserver.h"
#include "W25Q64.h"

#include "tusb_lwip_glue.h"
#include "lwipopts.h"

#include "lwip/apps/httpd.h"
#include "lwip/apps/fs.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/tcpip.h"
#include "filesys.h"

#define ADD_WEBSERVER 1

#define LED_PIN     25
// FLASH SPI
#define FLASH_PORT spi0
// SPI SELECT FLASH OR DISPLAY
#define SEL_FLASHB_DISP     28
#define FLASH_RX  16
#define FLASH_CS  17
#define FLASH_SCK 18
#define FLASH_TX  19
// #define FLASH_DATA_BLOCK 16
#define FLASH_BAUD 5 * 1000 * 1000
#define FLASH_READ_SIZE W25Q64_PAGE_SIZE*4
#define MAX_SPI_BUFFER_SIZE 3000


extern "C" int fs_open_custom(struct fs_file *file, const char *name);
extern "C" int fs_read_custom(struct fs_file *file, char *buffer, int count);
extern "C" void fs_close_custom(struct fs_file *file);
extern "C" u8_t fs_canread_custom(struct fs_file *file);
extern "C" u8_t fs_wait_read_custom(struct fs_file *file, fs_wait_cb callback_fn, void *callback_arg);
extern "C" int fs_read_async_custom(struct fs_file *file, char *buffer, int count, fs_wait_cb callback_fn, void *callback_arg);

/* CGI handler for LED control */
const char * cgi_handler_basic(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);
/* CGI handler for LED control with feedback*/
const char * cgi_handler_extended(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);
// CG Handler for POST
const char * cgi_handler_post(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);
// CGI Handler for DSP reset
const char * cgi_handler_resetDSP(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);
/* led control and debugging info */
void Led_On(int led);
void Led_Off(int led);

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
// Flash B
//FlashMemory flashB(FLASH_PORT, FLASH_RX, FLASH_TX, FLASH_CS, FLASH_SCK, SEL_FLASHB_DISP);

FlashFile * thisFile = NULL;
Webserver * _webserver = NULL;

//uint8_t spi_buffer[MAX_SPI_BUFFER_SIZE];
//uint16_t flashBufDataLen = 0;
//uint16_t sectorsWritten = 0;
//uint32_t fileSize = 0;

// struct fs_custom_data {
//   uint8_t * data_buffer;
//   int data_len;
// #if LWIP_HTTPD_FS_ASYNC_READ
//   int delay_read;
// //   fs_wait_cb callback_fn;
// //   void *callback_arg;
// #endif
// };

void Webserver::init(uint8_t (*action)(uint8_t, int16_t))
{
    // initialise for the first time
    _web_action = action;
    _webserver = this;

}

void Webserver::connect()
{    
    init_lwip();
    wait_for_netif_is_up();
    dhcpd_init();
    httpd_init();
    ssi_init();
    cgi_init();

}

void Webserver::serviceTraffic()
{
    tud_task();
    service_traffic();
}

uint8_t Webserver::doAction(uint8_t req, int16_t data)
{
    if (!_web_action) return 0;

    return _web_action(req, data);
}


const char html_header[] = "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>ToneDexter</title> "
        "<link href='https://unpkg.com/boxicons@2.1.2/css/boxicons.min.css' rel='stylesheet'>"
        "<script src=\"https://cdnjs.cloudflare.com/ajax/libs/jquery/3.2.1/jquery.min.js\"></script>"    
        "<script src=\"https://cdnjs.cloudflare.com/ajax/libs/bootbox.js/5.5.3/bootbox.min.js\"></script>"
        "<link href=\"https://use.fontawesome.com/releases/v5.3.1/css/all.css\" rel=\"stylesheet\" integrity=\"sha384-mzrmE5qonljUremFsqc01SB46JvROS7bZs3IO2EmfFsd15uHvIt+Y8vEf7N7fWAU\" crossorigin=\"anonymous\">"
        "<link href=\"https://fonts.googleapis.com/css2?family=Roboto\" rel=\"stylesheet\">" 
        "<link href=\"css/bootstrap.min.css\" rel=\"stylesheet\" />"
        "<script src=\"js/bootstrap.bundle.min.js\"></script>"
        "<script src=\"js/dselect.js\"></script>"    
        "<link rel=\"stylesheet\" href=\"css/josh_styles.css\">"
        "<script src=\"js/searchtool.js\"></script>"   
    "</head><body> <nav><div class=\"navbar\"><i class='bx bx-menu'></i>"
                "<div class=\"logo\"><a href=\"index.php\"><img src=\"images/AudioSprocketsLogoWhite.png\"></a></div>"
                "<div class=\"smalllogo\"><a href=\"index.php\"><img src=\"images/AudioSprocketsLogoWhite.png\"></a></div>"
                "<div class=\"nav-links\"><div class=\"sidebar-logo\"><span class=\"logo-name\"><img src=\"images/AudioSprocketsLogoWhite.png\"></span><i class=\"bx bx-x\"></i></div><ul class=\"links\">"
                    "<li><a href=\"/ssi.shtml\">Menu1</a></li><li><a href=\"/cgi.html\">Menu2</a></li><li><a href=\"/ssi_cgi.shtml\">Menu3</a></li></ul>"
            "</div></nav><div class=\"main-container\"><div class=\"row\"><div class=\"col-1\">";
     
const char html_footer[] = "</div><div class=\"col-2\"><img src=\"https://h4o15b.a2cdn1.secureserver.net/wp-content/uploads/2022/12/ToneDexter_II_VAbove_1000.jpg\" class=\"big-logo\">"
        "<div class=\"color-box\"></div></div></div><div class=\"footer\"><p>Copyright 2023 © Audio Sprockets</p></div></div></body></html>";

char response_url[20];

err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,
                 u16_t http_request_len, int content_len, char *response_uri,
                 u16_t response_uri_len, u8_t *post_auto_wnd)
{
    LWIP_UNUSED_ARG(connection);
    char * boundaryStart = strstr(http_request, "boundary=") + strlen("boundary=");
    LWIP_UNUSED_ARG(http_request_len);
    //fileSize = content_len; // add checks for max size here
    LWIP_UNUSED_ARG(post_auto_wnd);
    if (!memcmp(uri, "/test.cgi", 10)) {
        //testFile.setSize(content_len);
        thisFile = _webserver->getFilesystemHandle()->newFile();

        if (thisFile == NULL) return ERR_MEM;

        if (thisFile->open(FILE_WRITE) == FILESYS_OK) {
            thisFile->setWrapper(strtok(boundaryStart, "\r\n")); 
            //thisFile->setWrapperLen(boundaryLen + 4);
            strcpy(response_url, "/ssi.shtml");
            return ERR_OK;
        }
        else return ERR_INPROGRESS;
    }
    if (!memcmp(uri, "/dsp_update.cgi", 16)) {
        //testFile.setSize(content_len);
        thisFile = _webserver->getFilesystemHandle()->getFileHandle(DSP_FILE);
        thisFile->setAddress(0);
        
        if (thisFile->open(FILE_WRITE) == FILESYS_OK) {
            thisFile->setWrapper(strtok(boundaryStart, "\r\n")); 
            strcpy(response_url, "/success.html");
            //thisFile->setWrapperLen(boundaryLen + 4);
            return ERR_OK;
        }
        else return ERR_INPROGRESS;
    }
    return ERR_VAL;
}

err_t httpd_post_receive_data(void *connection, struct pbuf *p)
{
    LWIP_UNUSED_ARG(connection);

    uint16_t packet_length = p->len;
    uint8_t * packet_data = (uint8_t *)p->payload;

    // if (thisFile->isFirstPacket()) {
    //     packet_data += thisFile->getWrapperLen();
    //     packet_length -= thisFile->getWrapperLen();
    // }

    printf("\nPacket received");

    if (thisFile->write(packet_data, packet_length) == FILESYS_OK) return ERR_OK;
    
    return ERR_INPROGRESS;
    
}

void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len)
{
    // save the rest of the buffer
    thisFile->close();
    _webserver->getFilesystemHandle()->directoryUpdate();
    thisFile = NULL;
    snprintf(response_uri, response_uri_len, response_url);

}

//open custom file
int fs_open_custom(struct fs_file *file, const char *name) {
    const char generated_html[] =
"<div class=\"info-box\"><h1>Generated HTML</h1><div>This file has been generated from within the pico code</div>"
"<a href=\"/index.html\">Back</a><br></div>";

    const char text_entry[] = "Hello world";
    thisFile = NULL;
    uint16_t fileSize = sizeof(html_header) + sizeof(html_footer) + sizeof(generated_html);
    /* this example only provides one file */
    if (!strcmp(name, "/generated.html")) {
        /* initialize fs_file correctly */
        memset(file, 0, sizeof(struct fs_file));
        file->pextension = mem_malloc(fileSize);
        if (file->pextension != NULL) {
            /* instead of doing memcpy, you would generate e.g. a JSON here */
            strcpy((char *)file->pextension, html_header);
            strcat((char *)file->pextension, generated_html);
            strcat((char *)file->pextension, html_footer);

            file->data = (const char *)file->pextension;
            file->len = fileSize - 1; /* don't send the trailing 0 */
            file->index = file->len;
            /* allow persisteng connections */
            file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
            return 1;
        }
    }
    if (!strcmp(name, "/testing.txt")) {
        memset(file, 0, sizeof(struct fs_file));
        file->data = text_entry;
        file->len = sizeof(text_entry) - 1;
        file->index = file->len;
        file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
        return 1;
    }
    if (!strcmp(name, "/flashB.bin")) {
        thisFile = _webserver->getFilesystemHandle()->getFileHandle(1);

        if (thisFile == NULL) return 0;
        //struct fs_custom_data *data = (struct fs_custom_data *)mem_malloc(sizeof(struct fs_custom_data));
        //LWIP_ASSERT("out of memory?", data != NULL);
        memset(file, 0, sizeof(struct fs_file));
        thisFile->open(FILE_READ);
        file->len = 0; /* read size delayed */
        //data->delay_read = 3;
        //data->data_buffer = testFile.dataBuffer();
        //data->data_len = testFile.getSize();
        file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
        //file->pextension = data;

        return 1;
    }

    return 0;
}

// //read file and fill “buffer” step by step by “count” bytes amount
int fs_read_custom(struct fs_file *file, char *buffer, int count) {

    int read = 0;
    if (file->index >= file->len) return FS_READ_EOF;
    if (count > MAX_SPI_BUFFER_SIZE) return 0; // for now
    
    //read = (file->len - file->index < count ) ? file->len - file->index  : count;

    read = thisFile->read((uint8_t *)buffer, count);
    //uint8_t offset = flashB.readMemory(FLASH_DATA_BLOCK * W25Q64_BLOCK_SIZE + file->index, spi_buffer, read);
    // for (int i = 0; i < read; i++) {
    //     buffer[i] = spi_buffer[i + offset];
    // }       
    file->index += read;
    return read;
}

void fs_close_custom(struct fs_file *file) {

  if (file && file->pextension) {
    mem_free(file->pextension);
    file->pextension = NULL;
  }
//   if (file && file->pextension) {
//     //struct fs_custom_data *data = (struct fs_custom_data *)file->pextension;
//     //data->data_buffer = NULL;
//     //mem_free(data);
//     file->pextension = NULL;
//   }
    if (thisFile->isOpen()) {
        thisFile->close();
        thisFile == NULL;
    }     
}

u8_t fs_canread_custom(struct fs_file *file)
{
  /* This function is only necessary for asynchronous I/O:
     If reading would block, return 0 and implement fs_wait_read_custom() to call the
     supplied callback if reading works. */

    //struct fs_custom_data *data;
    LWIP_ASSERT("file != NULL", file != NULL);
    //data = (struct fs_custom_data *)file->pextension;
    if (thisFile == NULL) {
        /* file transfer has been completed already */
        LWIP_ASSERT("transfer complete", file->index == file->len);
        return 1;
    }
    LWIP_ASSERT("file != NULL", thisFile != NULL);
    /* The delay arises from SPI transfer */
    file->len = thisFile->getSize();
    if (thisFile->isDataReady()) {
        return 1;
    } else {
        return 0;
    }
    

    // if (data->delay_read == 3) {
    //     /* delayed file size mode */
    //     data->delay_read = 1;
    //     LWIP_ASSERT("", file->len == 0);
    //     file->len = testFile.getSize(); 
    //     data->delay_read = 1;
    //     return 0;
    // }
    // if (data->delay_read == 1) {
    //     /* tell read function to delay further */
    //     return 0;
    // }
    // LWIP_UNUSED_ARG(file);
    // return 1;
}


u8_t fs_wait_read_custom(struct fs_file *file, fs_wait_cb callback_fn, void *callback_arg)
{

    err_t err;
    //struct fs_custom_data *data = (struct fs_custom_data *)file->pextension;
    //LWIP_ASSERT("data not set", data != NULL);
    LWIP_UNUSED_ARG(file);
    LWIP_UNUSED_ARG(callback_fn);
    LWIP_UNUSED_ARG(callback_arg);
    /* Return
        - 1 if ready to read (at least one byte)
        - 0 if reading should be delayed (call 'tcpip_callback(callback_fn, callback_arg)' when ready) */
    return (thisFile->isDataReady()); 
}

int fs_read_async_custom(struct fs_file *file, char *buffer, int count, fs_wait_cb callback_fn, void *callback_arg)
{
    //struct fs_custom_data *data = (struct fs_custom_data *)file->pextension;
    //int read = (data->data_len - file->index < count ) ? data->data_len - file->index  : count;

    //LWIP_ASSERT("data not set", data != NULL);

    if (file->index >= thisFile->getSize()) return FS_READ_EOF;

    int read = thisFile->read((uint8_t *)buffer, count);

    //flashB.readMemory(FLASH_DATA_BLOCK * W25Q64_BLOCK_SIZE + file->index, (uint8_t *)buffer, read);
    //data->delay_read = 0;

    file->index += read;
    return read;
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
    },
    {
        "/reset_dsp.cgi", cgi_handler_resetDSP
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

const char * cgi_handler_resetDSP(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    _webserver->doAction(WEB_ACTION_RESET_DSP);

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
