#pragma once

#ifndef W25Q64_H
#define W25Q64_H

    #include <stdio.h>
    #include "pico/stdlib.h"
    #include "hardware/spi.h"
    //#include "Adafruit_SPIDevice.h"

    #define FLASH_MAX_BUFFER_SIZE 2048

    #define W25Q64_SELECT_DEVICE   1
    #define W25Q64_DESELECT_DEVICE 0

    #define W25Q64_MAX_BAUD     20 * 1000 * 1000  // testing indicates 4MHz at this stage
 
    // Instruction Set
    #define W25Q64_WR_ENABLE      0x06
    #define W25Q64_VOL_SR_WR_EN   0x50
    #define W25Q64_WR_DISABLE     0x04
    #define W25Q64_RELEASE_PWR_DN 0xAB
    #define W25Q64_MFR_DEV_ID     0x90
    #define W25Q64_JEDEC_ID       0x95
    #define W25Q64_READ_UNIQUE_ID 0x4B
    #define W25Q64_READ_DATA      0x03
    #define W25Q64_FAST_READ      0x0B
    #define W25Q64_PAGE_PROGRAM   0x02
    #define W25Q64_SECTOR_ERASE   0x20
    #define W25Q64_BLOCK_ERASE    0x52
    #define W25Q64_BLOCK64_ERASE  0xD8
    #define W25Q64_CHIP_ERASE     0xC7
    #define W25Q64_READ_SR1       0x05
    #define W25Q64_WRITE_SR1      0x01
    #define W25Q64_READ_SR2       0x35
    #define W25Q64_WRITE_SR2      0x31
    #define W25Q64_READ_SR3       0x15
    #define W25Q64_WRITE_SR3      0x11
    #define W25Q64_READ_SFDP_REG  0x5A
    #define W25Q64_ERASE_SEC_REG  0x44
    #define W25Q64_PROG_SEC_REG   0x42
    #define W25Q64_READ_SEC_REG   0x48
    #define W25Q64_GLOB_BL_LOK    0x7E
    #define W25Q64_GLOB_BL_ULOK   0x98
    #define W25Q64_READ_BL_LOK    0x3D
    #define W25Q64_IND_BL_LOK     0x36
    #define W25Q64_IND_BL_ULOK    0x39
    #define W25Q64_SUSPEND_ER_PR  0x75
    #define W25Q64_RESUME_ER_PR   0x75
    #define W25Q64_POWER_DOWN     0xB9
    #define W25Q64_ENABLE_RESET   0x66
    #define W25Q64_RESET_DEVICE   0x99

    // STATUS REGISTER 1
    #define W25Q64_SR1_SRP         7
    #define W25Q64_SR1_SEC         6
    #define W25Q64_SR1_TB          5
    #define W25Q64_SR1_BP2         4
    #define W25Q64_SR1_BP1         3
    #define W25Q64_SR1_BP0         2
    #define W25Q64_SR1_WEL         1
    #define W25Q64_SR1_BUSY        0

    // STATUS REGISTER 2
    #define W25Q64_SR2_SUS         7
    #define W25Q64_SR2_CMP         6
    #define W25Q64_SR2_LB3         5
    #define W25Q64_SR2_LB2         4
    #define W25Q64_SR2_LB1         3
    //#define W25Q64_SR2_RESERVED    2
    #define W25Q64_SR2_QE          1
    #define W25Q64_SR2_SRL         0

    // STATUS REGISTER 3
    //#define W25Q64_SR3_RESERVED    7
    #define W25Q64_SR3_DRV1        6
    #define W25Q64_SR3_DRV0        5
    //#define W25Q64_SR3_RESERVED    4
    //#define W25Q64_SR3_RESERVED    3
    #define W25Q64_SR3_WPS         2
    //#define W25Q64_SR3_RESERVED    1
    //#define W25Q64_SR3_RESERVED    0
    
    // MEMORY CONFIG
    #define W25Q64_PAGE_SIZE       0x100
    #define W25Q64_SECTOR_SIZE     0x1000  
    #define W25Q64_BLOCK_SIZE      0x10000 
    #define W25Q64_MAX_INSTR_LEN   5 

    #define W25Q64_PAGE_MASK       0x7FFF00
    #define W25Q64_SECTOR_MASK     0x7FF000 
    #define W25Q64_BLOCK_MASK      0x7F0000

    enum w25q64_drive_strength {drive_100, drive_75, drive_50, drive_25};

class W25Q64
{
    public:
        W25Q64(
                spi_inst_t *spi_chan = spi1,
                uint8_t spi_miso_pin = 8,
                uint8_t spi_mosi_pin = 11,
                uint8_t spi_cs_pin = 9,
                uint8_t spi_sck_pin = 10) : _spi(spi_chan), _miso_pin(spi_miso_pin), _mosi_pin(spi_mosi_pin), _cs_pin(spi_cs_pin), _sck_pin(spi_sck_pin) 
            { comms_buffer = new uint8_t[W25Q64_PAGE_SIZE + W25Q64_MAX_INSTR_LEN]; }
        ~W25Q64() { delete[] comms_buffer; }

        uint32_t initSPI(uint32_t baud);
        uint8_t setDriveStrength(w25q64_drive_strength strength);
        uint8_t read(uint32_t addr, uint8_t * data, size_t len);
        uint32_t getBaud() { return spi_get_baudrate(_spi); }  
        uint32_t setBaud(uint32_t baud) { return spi_set_baudrate(_spi, baud); }  

    protected :

        uint8_t eraseSector(uint32_t addr);
        uint8_t programPage(uint32_t addr, uint8_t * data, uint16_t len);    
         void setWordLen(uint8_t len) { spi_set_format(_spi, len, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST); }       
    private:

        spi_inst_t * _spi;
        const uint8_t _miso_pin;
        const uint8_t _mosi_pin;
        const uint8_t _cs_pin;
        const uint8_t _sck_pin;

        uint8_t *comms_buffer;

        void write_enable();
        uint8_t wait_done();

        int write_blocking(const uint8_t *src, size_t len);
        int write_read_blocking(const uint8_t *src, uint8_t *dst, size_t len);

};

class FlashMemory : public W25Q64
{
    public:
        FlashMemory(
                spi_inst_t *spi_chan = spi1,
                uint8_t spi_miso_pin = 8,
                uint8_t spi_mosi_pin = 11,
                uint8_t spi_cs_pin = 9,
                uint8_t spi_sck_pin = 10, 
                u_int8_t selectPin=28) : W25Q64(spi_chan, spi_miso_pin, spi_mosi_pin, spi_cs_pin, spi_sck_pin), _cs_pin(spi_cs_pin), _comms_sel_pin(selectPin) {}
        void init();
        void useBaud(uint32_t baud) { _requested_baud = (baud > W25Q64_MAX_BAUD ? W25Q64_MAX_BAUD : baud); }
        uint8_t readMemory(uint32_t addr, uint8_t * buffer, size_t len);
        uint8_t programMemory(uint32_t addr, uint8_t * buffer, size_t len);
    private:
        const uint8_t _comms_sel_pin;
        const uint8_t _cs_pin;
        uint32_t _requested_baud = W25Q64_MAX_BAUD;
        uint32_t _saved_baud;
        void selectDevice() { gpio_put(_comms_sel_pin, W25Q64_SELECT_DEVICE); }
        void deselectDevice() { gpio_put(_comms_sel_pin, W25Q64_DESELECT_DEVICE); }
        void prepareSpi();
        void saveBaud() { _saved_baud = getBaud(); }
        void restoreBaud() { setBaud(_saved_baud); }
        void restoreSpi();
};

#endif