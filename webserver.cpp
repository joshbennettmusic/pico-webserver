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
#define FLASH_DATA_BLOCK 16
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
uint8_t flashBuf[W25Q64_SECTOR_SIZE];
uint8_t spi_buffer[MAX_SPI_BUFFER_SIZE];
uint16_t flashBufDataLen = 0;
uint16_t sectorsWritten = 0;
uint32_t fileSize = 0;

struct fs_custom_data {
  uint8_t * data_buffer;
  int data_len;
#if LWIP_HTTPD_FS_ASYNC_READ
  int delay_read;
  fs_wait_cb callback_fn;
  void *callback_arg;
#endif
};



err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,
                 u16_t http_request_len, int content_len, char *response_uri,
                 u16_t response_uri_len, u8_t *post_auto_wnd)
{
    LWIP_UNUSED_ARG(connection);
    LWIP_UNUSED_ARG(http_request);
    LWIP_UNUSED_ARG(http_request_len);
    LWIP_UNUSED_ARG(content_len); // add checks for max size here
    LWIP_UNUSED_ARG(post_auto_wnd);
    if (!memcmp(uri, "/test.cgi", 10)) {
      return ERR_OK;
    }
    return ERR_VAL;
}

err_t httpd_post_receive_data(void *connection, struct pbuf *p)
{
    LWIP_UNUSED_ARG(connection);

    uint16_t packet_length = p->len;
    uint8_t * packet_data = (uint8_t *)p->payload;

    printf("\nPacket received");

    if (flashBufDataLen + packet_length >= W25Q64_SECTOR_SIZE) {
        // we've got enough data to fill a sector
        printf("\nBuffer full... ");
        uint16_t bufferRoomLeft = W25Q64_SECTOR_SIZE - flashBufDataLen;
        memcpy(&flashBuf[flashBufDataLen], p->payload, bufferRoomLeft);
        flashB.programMemory(FLASH_DATA_BLOCK * W25Q64_BLOCK_SIZE + sectorsWritten * W25Q64_SECTOR_SIZE, flashBuf, W25Q64_SECTOR_SIZE);
        printf("Packets saved to sector  %d\n",sectorsWritten);
        sectorsWritten++;
        flashBufDataLen = packet_length - bufferRoomLeft;
        if (flashBufDataLen) memcpy(flashBuf, &packet_data[bufferRoomLeft], packet_length);

    } else {
        // add this packet to the flash write buffer
        memcpy(&flashBuf[flashBufDataLen],p->payload,packet_length);
        flashBufDataLen += packet_length;

    }
    
    return ERR_OK;
    
}

void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len)
{
  /* default page is "login failed" */
  // save the rest of the buffer
  if (flashBufDataLen) {
    flashB.programMemory(FLASH_DATA_BLOCK * W25Q64_BLOCK_SIZE + sectorsWritten * W25Q64_SECTOR_SIZE, flashBuf, flashBufDataLen);
    printf("Final %d bytes saved to sector  %d\n",flashBufDataLen, sectorsWritten);
    fileSize = sectorsWritten * W25Q64_SECTOR_SIZE + flashBufDataLen;
    printf("\nBuffer Out Start:\n");
    for (int i = 0; i < 256; i++) {
        printf("%02X",flashBuf[i]); 

        if (i % 16 == 15)
            printf("\n");
        else
            printf(" ");

    }
    printf("\nFlash Memory Start:\n");
    uint16_t offset = flashB.readMemory(FLASH_DATA_BLOCK * W25Q64_BLOCK_SIZE, flashBuf, 256);
    //uint16_t offset = flashB.readMemory(0, flashBuf, 256);
    for (int i = offset; i < 256 + offset; i++) {
        if ((flashBuf[i] > 33) && (flashBuf[i] < 127)) {
            printf("%c ",flashBuf[i]); 
        } else {
            printf("%02X",flashBuf[i]); 
        }
        

        if (i % 16 == 3)
            printf("\n");
        else
            printf(" ");

    }    
  } 
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
        struct fs_custom_data *data = (struct fs_custom_data *)mem_malloc(sizeof(struct fs_custom_data));
        LWIP_ASSERT("out of memory?", data != NULL);
        memset(file, 0, sizeof(struct fs_file));
        file->len = 0; /* read size delayed */
        data->delay_read = 3;
        data->data_buffer = spi_buffer;
        data->data_len = 1179771;
        file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
        file->pextension = data;
        //file->pextension = mem_malloc(FLASH_READ_SIZE + 10);
        //file->pextension = NULL;
        //file->data = "";
        //file->len = 65970;
        //ile->index = 0;

        //file->is_custom_file = 1;
        return 1;
    }

    return 0;
}

// //read file and fill “buffer” step by step by “count” bytes amount
int fs_read_custom(struct fs_file *file, char *buffer, int count) {

    int read = 0;
    if (file->index >= file->len) return FS_READ_EOF;
    if (count > MAX_SPI_BUFFER_SIZE) return 0; // for now
    
    read = (file->len - file->index < count ) ? file->len - file->index  : count;
    uint8_t offset = flashB.readMemory(FLASH_DATA_BLOCK * W25Q64_BLOCK_SIZE + file->index, spi_buffer, read);
    for (int i = 0; i < read; i++) {
        buffer[i] = spi_buffer[i + offset];
    }       
    file->index += read;
    return read;
}

void fs_close_custom(struct fs_file *file) {

//   if (file && file->pextension) {
//     mem_free(file->pextension);
//     file->pextension = NULL;
//   }
  if (file && file->pextension) {
    struct fs_custom_data *data = (struct fs_custom_data *)file->pextension;
    data->data_buffer = NULL;
    mem_free(data);
    file->pextension = NULL;
  }
}

u8_t fs_canread_custom(struct fs_file *file)
{
  /* This function is only necessary for asynchronous I/O:
     If reading would block, return 0 and implement fs_wait_read_custom() to call the
     supplied callback if reading works. */

    struct fs_custom_data *data;
    LWIP_ASSERT("file != NULL", file != NULL);
    data = (struct fs_custom_data *)file->pextension;
    if (data == NULL) {
        /* file transfer has been completed already */
        LWIP_ASSERT("transfer complete", file->index == file->len);
        return 1;
    }
    LWIP_ASSERT("data != NULL", data != NULL);
    /* The delay arises from SPI transfer */
    if (data->delay_read == 3) {
        /* delayed file size mode */
        data->delay_read = 1;
        LWIP_ASSERT("", file->len == 0);
        file->len = data->data_len;
        data->delay_read = 1;
        return 0;
    }
    if (data->delay_read == 1) {
        /* tell read function to delay further */
        return 0;
    }
    LWIP_UNUSED_ARG(file);
    return 1;
}

// static void fs_spi_read_cb(void *arg)
// {
//   struct fs_custom_data *data = (struct fs_custom_data *)arg;
//   fs_wait_cb callback_fn = data->callback_fn;
//   void *callback_arg = data->callback_arg;
//   data->callback_fn = NULL;
//   data->callback_arg = NULL;

//   LWIP_ASSERT("no callback_fn", callback_fn != NULL);

//   callback_fn(callback_arg);
// }

u8_t fs_wait_read_custom(struct fs_file *file, fs_wait_cb callback_fn, void *callback_arg)
{

    err_t err;
    struct fs_custom_data *data = (struct fs_custom_data *)file->pextension;
    LWIP_ASSERT("data not set", data != NULL);
    //data->callback_fn = callback_fn;
    //data->callback_arg = callback_arg;
    //err = tcpip_try_callback(fs_spi_read_cb, data);
    //LWIP_ASSERT("out of queue elements?", err == ERR_OK);
    //LWIP_UNUSED_ARG(err);

    LWIP_UNUSED_ARG(file);
    LWIP_UNUSED_ARG(callback_fn);
    LWIP_UNUSED_ARG(callback_arg);
    /* Return
        - 1 if ready to read (at least one byte)
        - 0 if reading should be delayed (call 'tcpip_callback(callback_fn, callback_arg)' when ready) */
    return (data->delay_read == 0 ? 1 : 0);
}

int fs_read_async_custom(struct fs_file *file, char *buffer, int count, fs_wait_cb callback_fn, void *callback_arg)
{
    struct fs_custom_data *data = (struct fs_custom_data *)file->pextension;
    int read = (data->data_len - file->index < count ) ? data->data_len - file->index  : count;

    LWIP_ASSERT("data not set", data != NULL);

    if (file->index >= data->data_len) return FS_READ_EOF;

    /* This delay would normally come e.g. from SPI transfer */
    // LWIP_ASSERT("invalid state", data->delay_read >= 0 && data->delay_read <= 2);
    // if (data->delay_read == 2) {
    //     /* no delay next time */
    //     data->delay_read = 0;
    //     return FS_READ_DELAYED;
    // } else if (data->delay_read == 1) {
    //     err_t err;
    //     /* execute requested delay */
    //     data->delay_read = 2;
    //     LWIP_ASSERT("duplicate callback request", data->callback_fn == NULL);
    //     data->callback_fn = callback_fn;
    //     data->callback_arg = callback_arg;
    //     //tcpip_callback(fs_example_read_cb, data);
    //     //LWIP_ASSERT("out of queue elements?", err == ERR_OK);
    //     //LWIP_UNUSED_ARG(err);
    //     return FS_READ_DELAYED;
    // }
    /* execute this read but delay the next one */

    flashB.readMemory(FLASH_DATA_BLOCK * W25Q64_BLOCK_SIZE + file->index, (uint8_t *)buffer, read);
    data->delay_read = 0;
    //callback_fn(callback_arg);    
    // LWIP_UNUSED_ARG(callback_fn);
    // LWIP_UNUSED_ARG(callback_arg);

    file->index += read;

    /* Return
        - FS_READ_EOF if all bytes have been read
        - FS_READ_DELAYED if reading is delayed (call 'tcpip_callback(callback_fn, callback_arg)' when done) */

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
