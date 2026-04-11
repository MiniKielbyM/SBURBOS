#ifndef SBURB_SERIALLIB_H
#define SBURB_SERIALLIB_H
#include <stdint.h>
#include "SBURB_portlib.h"
void serial_init();
void serial_write(char c);
void serial_write_string(const char *str);
void hcf();
#endif // SBURB_SERIALLIB_H