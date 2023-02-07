#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include <string.h>
#include "pico/bootrom.h"
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"
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

// DSP RESET
#define DSP_RST_N           6

extern "C" int fs_open_custom(struct fs_file *file, const char *name);
extern "C" int fs_read_custom(struct fs_file *file, char *buffer, int count);
extern "C" void fs_close_custom(struct fs_file *file);
extern "C" u8_t fs_canread_custom(struct fs_file *file);
extern "C" u8_t fs_wait_read_custom(struct fs_file *file, fs_wait_cb callback_fn, void *callback_arg);
extern "C" int fs_read_async_custom(struct fs_file *file, char *buffer, int count, fs_wait_cb callback_fn, void *callback_arg);

// Flash B
FlashMemory flashB(FLASH_PORT, FLASH_RX, FLASH_TX, FLASH_CS, FLASH_SCK, SEL_FLASHB_DISP);
FlashFile testFile(&flashB);
FlashFile * thisFile = NULL;


//uint8_t spi_buffer[MAX_SPI_BUFFER_SIZE];
//uint16_t flashBufDataLen = 0;
//uint16_t sectorsWritten = 0;
uint32_t fileSize = 0;

// struct fs_custom_data {
//   uint8_t * data_buffer;
//   int data_len;
// #if LWIP_HTTPD_FS_ASYNC_READ
//   int delay_read;
// //   fs_wait_cb callback_fn;
// //   void *callback_arg;
// #endif
// };



err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,
                 u16_t http_request_len, int content_len, char *response_uri,
                 u16_t response_uri_len, u8_t *post_auto_wnd)
{
    LWIP_UNUSED_ARG(connection);
    char * boundaryStart = strstr(http_request, "boundary=") + strlen("boundary=");
    
    LWIP_UNUSED_ARG(http_request_len);
    fileSize = content_len; // add checks for max size here
    LWIP_UNUSED_ARG(post_auto_wnd);
    if (!memcmp(uri, "/test.cgi", 10)) {
        //testFile.setSize(content_len);
        thisFile = &testFile;

        if (thisFile->open(FILE_WRITE) == FILESYS_OK) {
            thisFile->setWrapper(strtok(boundaryStart, "\r\n")); 
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
    thisFile = NULL;
    snprintf(response_uri, response_uri_len, "/ssi.shtml");

}

//open custom file
int fs_open_custom(struct fs_file *file, const char *name) {
    const char generated_html[] =
"<html>\n"
"<head><title>lwIP - A Lightweight TCP/IP Stack</title></head>\n"
" <body bgcolor=\"white\" text=\"black\">\n"
"  <table width=\"100%\">\n"
"   <tr valign=\"top\">\n"
"    <td width=\"80\">\n"
"     <a href=\"http://www.sics.se/\"><img src=\"/img/sics.gif\"\n"
"      border=\"0\" alt=\"SICS logo\" title=\"SICS logo\"></a>\n"
"    </td>\n"
"    <td width=\"500\">\n"
"     <h1>lwIP - A Lightweight TCP/IP Stack</h1>\n"
"     <h2>Generated page</h2>\n"
"     <p>This page might be generated in-memory at runtime</p>\n"
"    </td>\n"
"    <td>\n"
"    &nbsp;\n"
"    </td>\n"
"   </tr>\n"
"  </table>\n"
" </body>\n"
"</html>";

    const char text_entry[] = "Hello world";
    thisFile = NULL;

    /* this example only provides one file */
    if (!strcmp(name, "/generated.html")) {
        /* initialize fs_file correctly */
        memset(file, 0, sizeof(struct fs_file));
        file->pextension = mem_malloc(sizeof(generated_html));
        if (file->pextension != NULL) {
            /* instead of doing memcpy, you would generate e.g. a JSON here */
            memcpy(file->pextension, generated_html, sizeof(generated_html));
            file->data = (const char *)file->pextension;
            file->len = sizeof(generated_html) - 1; /* don't send the trailing 0 */
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
    if (!strcmp(name, "/image.jpg")) {
        thisFile = &testFile;
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

//   if (file && file->pextension) {
//     mem_free(file->pextension);
//     file->pextension = NULL;
//   }
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


uint8_t testFlashB() {

    uint8_t mismatch;
    uint8_t random_data[W25Q64_PAGE_SIZE+20];
    uint8_t readback_data[W25Q64_PAGE_SIZE+20];
    uint8_t test_block = 0;
    uint8_t errors = 0;

    printf("Flash B Test\nRandom Numbers generated:\n");
    for (int i = 0; i < W25Q64_PAGE_SIZE; i++) {
        random_data[i] = rand() >> 16;
        printf("%02X",random_data[i]);
        if (i % 16 == 15)
            printf("\n");
        else
            printf(" ");
        
    }

    if (!flashB.programMemory(uint32_t(test_block << 16),random_data, W25Q64_PAGE_SIZE)) {
        printf("\nProgram Failed!\n");
    }

    // read back
    uint8_t offset = flashB.readMemory(uint32_t(test_block<<16),readback_data, W25Q64_PAGE_SIZE);
    //uint8_t offset = flashB.readMemory(0,readback_data,W25Q64_PAGE_SIZE);
    printf("Erase, Program and Read of Flash Complete\nData returned: \n\n");

    mismatch = 0;

    for (int i = 0; i < W25Q64_PAGE_SIZE; i++) {
        if (random_data[i] != readback_data[i+offset]){
            if (!mismatch) mismatch = i;

        }
        printf("%02X",readback_data[i+offset]);
        //printf("%02X",readback_data[i+4]);
        if (i % 16 == 15)
            printf("\n");
        else
            printf(" ");
        
    }

    if (mismatch) {
        printf("\nERROR! - data mismatch at location %i\n", mismatch);
        errors++;
    } else {
        printf("\nSuccess! - data programmed correctly\n");
    }  

    printf("\nTest Complete, with %i errors\n", errors);
    return 0;
}


int main()
{
    // initialise IO and devices
    stdio_init_all();

    flashB.init();
    flashB.initSPI(FLASH_BAUD);

    // initialise for the first time
    if (!testFile.init()) {
        testFile.setAddress(FLASH_DATA_BLOCK * W25Q64_BLOCK_SIZE);
        testFile.setSize(34311);
        testFile.update();
    }
    // Initialize tinyusb, lwip, dhcpd and httpd
#if ADD_WEBSERVER
    init_lwip();
    wait_for_netif_is_up();
    dhcpd_init();
    httpd_init();
    ssi_init();
    cgi_init();
#endif
    //http_set_cgi_handlers(cgi_handlers, LWIP_ARRAYSIZE(cgi_handlers));
    
    // For toggle_led
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);

    // DSP on to stop it oscillating
    gpio_init(DSP_RST_N);
    gpio_set_dir(DSP_RST_N, GPIO_OUT);
    gpio_put(DSP_RST_N, 1);

    testFlashB();

    printf("\nInitialised, waiting for webserver data...");
    while (true)
    {
#if ADD_WEBSERVER
        tud_task();
        service_traffic();
#endif
    }

    return 0;
}
