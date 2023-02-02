#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/sync.h"

#include "W25Q64.h"

uint32_t W25Q64::initSPI(uint32_t baud) {


	gpio_set_function(_miso_pin, GPIO_FUNC_SPI);
	gpio_set_function(_mosi_pin, GPIO_FUNC_SPI);
	gpio_set_function(_sck_pin, GPIO_FUNC_SPI);
	//gpio_set_function(_cs_pin, GPIO_FUNC_SPI);   
 	uint32_t setBaud = spi_init(_spi, baud > W25Q64_MAX_BAUD ? W25Q64_MAX_BAUD : baud);
	spi_set_format(_spi, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
    return setBaud;
}

void W25Q64::write_enable() {
    comms_buffer[0] = W25Q64_WR_ENABLE;
    write_blocking(comms_buffer,1);
    //sleep_us(500);
}

uint8_t W25Q64::wait_done() {
    uint16_t timeout_counter = 0;
    uint8_t readback[2];

    comms_buffer[0] = W25Q64_READ_SR1;
    readback[1] = 0xFF;
    while (readback[1] & (1 << W25Q64_SR1_BUSY)) {
        write_read_blocking(comms_buffer, readback, 2);
        timeout_counter++;
        if (timeout_counter > 0xFFF0) {
                return 0;
        }
        sleep_us(100);
    }
    return 1;
}

uint8_t W25Q64::setDriveStrength(w25q64_drive_strength strength) {
    uint8_t retval = 0;
    write_enable();
    comms_buffer[0] = W25Q64_WRITE_SR3;
    comms_buffer[1] = uint8_t(strength << W25Q64_SR3_DRV0);    
    write_blocking(comms_buffer,2);
    retval = wait_done();
    return retval;    

}

uint8_t W25Q64::eraseSector(uint32_t addr) {

    // address must be a multiple of 0x10000, so ignore last 12 digits of the address
    addr &= W25Q64_SECTOR_MASK;
    
    write_enable();
    sleep_us(100);
    comms_buffer[0] = W25Q64_SECTOR_ERASE;
    comms_buffer[1] = uint8_t(addr >> 16);
    comms_buffer[2] = uint8_t(addr >> 8);
    comms_buffer[3] = uint8_t(addr);

    write_blocking(comms_buffer, 4);
    sleep_us(100);
    return wait_done();
}


uint8_t W25Q64::programPage(uint32_t addr, uint8_t * data, uint16_t len) {
    const uint8_t header_len = 4;
    write_enable();
    sleep_us(100);
    comms_buffer[0] = W25Q64_PAGE_PROGRAM; 
    comms_buffer[1] = uint8_t(addr >> 16);
    comms_buffer[2] = uint8_t(addr >> 8);
    comms_buffer[3] = uint8_t(addr);
    for (int i = 0; i < len; i++) {
        comms_buffer[header_len + i] = data[i];
    }
    write_blocking(comms_buffer, len + header_len);
    sleep_us(100);
    return  wait_done();
}


uint8_t W25Q64::read(uint32_t addr, uint8_t * data, size_t len) {

    const uint8_t header_len = 4;
    comms_buffer[0] = W25Q64_READ_DATA; 
    comms_buffer[1] = uint8_t(addr >> 16);
    comms_buffer[2] = uint8_t(addr >> 8);
    comms_buffer[3] = uint8_t(addr);
    
    write_read_blocking(comms_buffer, data, len + header_len);
    return header_len;
}

int W25Q64::write_blocking(const uint8_t *src, size_t len)
{
    int retval;
    uint32_t interrupt_status = save_and_disable_interrupts();
    //gpio_put(_cs_pin, 0);
    retval = spi_write_blocking(_spi, src, len);
    //gpio_put(_cs_pin, 1);
    restore_interrupts(interrupt_status);
    return retval;
}

int W25Q64::write_read_blocking(const uint8_t *src, uint8_t *dst, size_t len)
{
    int retval;
    uint32_t interrupt_status = save_and_disable_interrupts();
    //gpio_put(_cs_pin, 0);
    retval = spi_write_read_blocking(_spi, src, dst, len);
    //gpio_put(_cs_pin, 1);
    restore_interrupts(interrupt_status);
    return retval;
}

void FlashMemory::init() {
    gpio_init(_comms_sel_pin);
    gpio_set_dir(_comms_sel_pin, GPIO_OUT);
    gpio_put(_comms_sel_pin, W25Q64_DESELECT_DEVICE);
}

uint8_t FlashMemory::readMemory(uint32_t addr, uint8_t * buffer, size_t len)
{
    prepareSpi();
    uint8_t dataOffset = read(addr, buffer, len);
    restoreSpi();
    return dataOffset;
}

uint8_t FlashMemory::programMemory(uint32_t addr, uint8_t * buffer, size_t len)
{
    // add stuff here to make the data safe, but make simple for now
    uint8_t retval;

    size_t lenRemaining = len;
    size_t thisLen;
    uint8_t * bufPtr = buffer;
    uint32_t thisAddr = addr;    

    uint32_t thisSector = (addr & W25Q64_SECTOR_MASK);
    uint32_t thisPage = (addr & W25Q64_PAGE_MASK);
    uint32_t lastPage = (addr + len) & W25Q64_PAGE_MASK;


    prepareSpi();
    // check that data to read doesn't cross sector boundary
    while ((addr + len) > thisSector) {
        // may require more than one erase instruction
        // erase this sector
        retval = eraseSector(thisSector);
        if (!retval) {
            restoreSpi();
            return retval;
        }
        thisSector += W25Q64_SECTOR_SIZE;
    }
    // give it some time between successive writes
    sleep_us(100);

    while ((lenRemaining) > 0) {
        if (lastPage - thisPage) {
            thisLen = thisPage - thisAddr + W25Q64_PAGE_SIZE;
            // write up to the page boundary only
        } else {
            // last page worth of data (may be less than total)
            thisLen = lenRemaining;
        }
        retval = programPage(thisAddr, bufPtr, thisLen );
        thisPage += W25Q64_PAGE_SIZE;
        lenRemaining -= thisLen;
        thisAddr = thisPage;
        bufPtr += thisLen;        
    }
    restoreSpi();
    return retval;
}

void FlashMemory::prepareSpi()
{
    //gpio_init(_cs_pin);
    //gpio_set_dir(_cs_pin, GPIO_OUT);
    gpio_set_function(_cs_pin, GPIO_FUNC_SPI);
    // uint32_t currentBaud = getBaud();
    // if (currentBaud != _requested_baud) {
    //     saveBaud();
    //     currentBaud = setBaud(_requested_baud);
    // }
    setWordLen(8);
    selectDevice();
}

void FlashMemory::restoreSpi()
{
    gpio_set_function(_cs_pin, GPIO_FUNC_SIO);
    // restoreBaud();
    deselectDevice();
}

