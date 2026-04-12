#ifndef SBURB_MOUSELIB_H
#define SBURB_MOUSELIB_H

#include "SBURB_portlib.h"
#include "SBURB_strlib.h"
#include "SBURB_fontlib.h"
#include "SBURB_globals.h"
#include <stdint.h>

struct mouse_event {
    int32_t x;
    int32_t y;
    uint8_t button;
    enum {
        MOUSE_MOVE,
        MOUSE_CLICK
    } type;
};


void mouse_init();
void mouse_handler(struct mouse_event event);

#endif // SBURB_MOUSELIB_H