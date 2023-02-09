#pragma once

#ifndef WEBSERVER_H

#define WEBSERVER_H

#include "tusb_lwip_glue.h"
#include "lwipopts.h"

#include "lwip/apps/httpd.h"
#include "lwip/apps/fs.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/tcpip.h"
#include "filesys.h"
#include "W25Q64.h"


// Actions
#define WEB_ACTION_RESET_DSP 0
// more...




class Webserver {
    public:
        Webserver(FileSys * fileSystemHandle) : _fileSystem(fileSystemHandle) 
        {            
 
        }
        ~Webserver() 
        {

        }
        void init(uint8_t (*action)(uint8_t, int16_t));
        void connect(void);
        void serviceTraffic(void);
        FileSys * getFilesystemHandle() const { return _fileSystem; } 
        uint8_t doAction(uint8_t req, int16_t data = 0);
    private:
        FileSys * _fileSystem;
        uint8_t (*_web_action)(uint8_t, int16_t) = 0;

};


#endif
