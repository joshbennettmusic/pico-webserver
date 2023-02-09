#include "filesys.h"
#include <string.h>

#define FLASH_DIRECTORY_BLOCK 15

#define ADDR_OFFSET 0
#define SIZE_OFFSET 1
#define CHECK_OFFSET 2
#define FS_INDEX_OFFSET 3

uint8_t flashBuf[W25Q64_SECTOR_SIZE];
char boundaryText[70]; //maximum allowed size;

uint8_t FlashFile::init(uint8_t fsIndex)
{
    _filesysIndex = fsIndex;
    int fileBuf[4];
    int bufSize = sizeof(fileBuf);
    _flash->readMemory(FLASH_DIRECTORY_BLOCK * W25Q64_BLOCK_SIZE + _filesysIndex * sizeof(fileBuf), (uint8_t *)fileBuf, bufSize );

    if (fileBuf[CHECK_OFFSET] != FILESYS_CHK) return FILESYS_ERR;

    _address = (int)fileBuf[ADDR_OFFSET];
    _size = (int)fileBuf[SIZE_OFFSET];
    _filesysIndex = (int)fileBuf[FS_INDEX_OFFSET];
    return FILESYS_OK;
}

uint8_t FlashFile::stripWrapper()
{
    uint8_t index;
    // // remove boundary text
    // _address += strlen(boundaryText) + 4;
    // _size -= strlen(boundaryText) + 4;
    // // remove header
    // _flash->readMemory(_address, flashBuf, W25Q64_PAGE_SIZE);
    // uint16_t headerLen = strlen(strtok((char *)flashBuf, "\r\n\r\n")) + 4;
    // _address += headerLen;
    // _size -= headerLen;
    // remove footer
    _flash->readMemory(_address + _size - W25Q64_PAGE_SIZE, flashBuf, W25Q64_PAGE_SIZE);
    
    for (uint8_t index = 0; index <  W25Q64_PAGE_SIZE - strlen(boundaryText); index++) {
        if (!memcmp(&flashBuf[index], boundaryText, strlen(boundaryText))) {
            _size -= W25Q64_PAGE_SIZE - index + 4;
            break;
        }
    }    
    return FILESYS_OK;

}

uint8_t FlashFile::open(uint8_t mode)
{
    if (_status != STATUS_IDLE) return FILESYS_ERR;
    if (mode == FILE_WRITE) {
        //_address = getBaseAddr();
        _size = 0;
        _index = 0;
        _status = FILE_WRITE;
        return FILESYS_OK;
    } 
    if (mode == FILE_READ) {
        _status = STATUS_NO_DATA_YET;
        _index = 0;
        return FILESYS_OK;
    }
    return FILESYS_ERR;
}

uint8_t FlashFile::update()
{
    int fileBuf[4];
    int bufSize = sizeof(fileBuf);

    fileBuf[CHECK_OFFSET] = FILESYS_CHK;
    fileBuf[ADDR_OFFSET] = _address;
    fileBuf[SIZE_OFFSET] = _size;
    _flash->programMemory(FLASH_DIRECTORY_BLOCK * W25Q64_BLOCK_SIZE, (uint8_t *)fileBuf, bufSize);
    return FILESYS_OK;
}

uint8_t FlashFile::write(uint8_t * data, uint16_t len)
{
    if (_status != FILE_WRITE) return FILESYS_ERR;

    // first thing to be received is header rubbish, wait for \r\n\r\n
    if (_index == 0 && _size == 0) {
        // no file yet, so look for header end
        for (uint16_t i = 0; i < len - 4; i++) {
            if (!memcmp(&data[i],"\r\n\r\n",4)) {
                // found end of header
                data += i + 4;
                len -= i + 4;            
                break;    
            }
        }
    }

    if (_index + len >= W25Q64_SECTOR_SIZE)
    {
        // buffer can be filled
        uint16_t bytesAdded = W25Q64_SECTOR_SIZE - _index;
        memcpy(flashBuf + _index, data, bytesAdded);
        _flash->programMemory(_address + _size, flashBuf, W25Q64_SECTOR_SIZE);
        _index = len - bytesAdded;
        _size += W25Q64_SECTOR_SIZE;
        // add remaining data to the beginmomg of the buffer
        memcpy(flashBuf, data + bytesAdded, _index);
    } else {
        memcpy(&flashBuf[_index], data, len);
        _index += len;
    }
    return FILESYS_OK;
}

uint16_t FlashFile::read(uint8_t * data, uint16_t len)
{
    //if (_status != STATUS_NO_DATA_YET) return FILESYS_ERR;

    uint16_t bytesRead = (_size - _index < len) ? _size - _index : len;
    _flash->readMemory(_address + _index, data, bytesRead);
    _index += bytesRead;
    _status = STATUS_DATA_READY;
    return bytesRead;
}

// uint8_t * FlashFile::dataBuffer(void) {
//     return flashBuf;
// }
uint8_t FlashFile::close()
{
    if (_status == FILE_WRITE) {
        // write the remaining bytes in the buffer
        if (_index) _flash->programMemory(_address + _size, flashBuf, _index);
        _size += _index;
        _index = 0;

        // now have the full file in Flash, including wrappers
        stripWrapper();
        //update();
    }
    _status = STATUS_IDLE;
    return FILESYS_OK;
}

bool FlashFile::isDataReady()
{ 
    if (_status == STATUS_DATA_READY) {
        //_status = STATUS_NO_DATA_YET;
        return true;
    }

    return false;
}

void FlashFile::setWrapper(const char * wrapper)
{
    strcpy(boundaryText, wrapper);
}

uint8_t FileSys::init()
{
    FlashFile * checkFile = new FlashFile(_flash);
    for (int i = 0; i < FILE_COUNT_MAX; i++) {
        if (checkFile->init(i) == FILESYS_OK) {
            // file exists on Flash
            _files[i] = checkFile;
            _file_count++;
            checkFile = new FlashFile(_flash);
        }
    }
    return FILESYS_OK;
}

FlashFile * FileSys::newFile(int addr, int size)
{
    if (_file_count >= FILE_COUNT_MAX) return NULL;

    _files[_file_count] = new FlashFile(_flash, _file_count);
    if (addr == ADDR_NEXT_AVAIL) {
        // dp some fancy allocation, but probably not needed for TD
        int nextAddr = W25Q64_SECTOR_SIZE + W25Q64_SECTOR_MASK & (_files[_file_count - 1]->getAddress() + _files[_file_count - 1]->getSize());
        if (nextAddr < FLASH_DATA_BLOCK * W25Q64_BLOCK_SIZE) nextAddr = FLASH_DATA_BLOCK * W25Q64_BLOCK_SIZE;
        _files[_file_count]->setAddress(nextAddr);
    } else {
        _files[_file_count]->setAddress(addr);
    }
    _file_count++;
    return _files[_file_count-1];

}

void FileSys::deleteFile(FlashFile * thisFile)
{
    // bare minimum
    for (int i = thisFile->getFilesysIndex(); i < _file_count - 1; i++ ) {
        _files[i] = _files[i + 1];
        _files[i]->setFilesysIndex(i);
    }
    _file_count--;
    delete thisFile;
    directoryUpdate();

}

void FileSys::directoryUpdate() {
    int fileBuf[4];
    int bufSize = sizeof(fileBuf);


    for (uint8_t i = 0; i < _file_count; i++) {
        fileBuf[CHECK_OFFSET] = FILESYS_CHK;
        fileBuf[ADDR_OFFSET] = _files[i]->getAddress();
        fileBuf[SIZE_OFFSET] = _files[i]->getSize();
        fileBuf[FS_INDEX_OFFSET] = i;
        memcpy(flashBuf + i * bufSize, fileBuf, bufSize);
    }
    _flash->programMemory(FLASH_DIRECTORY_BLOCK * W25Q64_BLOCK_SIZE, flashBuf, bufSize * _file_count);
    
}