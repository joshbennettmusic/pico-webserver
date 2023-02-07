#pragma once

#ifndef FLASH_FILE_H
#define FLASH_FILE_H

    #include <stdio.h>
    #include "pico/stdlib.h"
    #include "W25Q64.h"

    #define FILESYS_OK 1
    #define FILESYS_ERR 0 // elaborate later if necessary

    #define FILE_WRITE 1
    #define FILE_READ  0
    #define STATUS_IDLE  2
    #define STATUS_DATA_READY 3
    #define STATUS_NO_DATA_YET 4

    #define FILESYS_CHK 0x55

    #define FLASH_DATA_BLOCK 16
    
    class FlashFile {
        public:
            FlashFile(FlashMemory * flashMemory) : _flash(flashMemory) {}
            ~FlashFile() {}
            uint8_t init();
            uint8_t stripWrapper(void);
            int getAddress(void) const { return _address; }
            int getBaseAddr(void) const { return FLASH_DATA_BLOCK * W25Q64_BLOCK_SIZE; }
            void setAddress(int addr) { _address = addr; }
            int getSize(void) const { return _size; }
            void setSize(int size) { _size = size; }  // add constraints later
            uint8_t open(uint8_t mode);
            uint8_t write(uint8_t * data, uint16_t len);
            uint16_t read(uint8_t * data, uint16_t len);
            //uint8_t * dataBuffer(void);
            uint8_t close(void);
            uint8_t update(void);
            bool isOpen(void) { return _status != STATUS_IDLE; }
            bool isDataReady(void); 
            void setWrapper(const char * wrapper);
            //void setWrapperLen(uint16_t len) { _wrapperLen = len; }
            //uint16_t getWrapperLen(void) const { return _wrapperLen; }
            //bool isFirstPacket() { return ((_size == 0) && (_index == 0)); }
        private:
            FlashMemory * _flash;
            uint8_t _status = STATUS_IDLE;
            int _address;
            int _size;
            int _index = 0;
            uint16_t _wrapperLen = 0;
    };

#endif