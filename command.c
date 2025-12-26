#include "command.h"

void pack_header(uint8_t* buffer, uint16_t length, uint16_t func_code, uint8_t flags) {
    buffer[0] = (length >> 4) & 0xFF;
    buffer[1] = ((length & 0x0F) << 4) | ((func_code >> 8) & 0x0F);
    buffer[2] = func_code & 0xFF;
    buffer[3] = flags;
}

int unpack_header(uint8_t * buffer, uint16_t * len, uint16_t * func_code, uint8_t * flags)
{
  if(!buffer || !(len || func_code || flags ) ) return -1;
  
  if(len) *len = (uint16_t)(buffer[0]<<4) | (buffer[1] >> 4);
  if(func_code) *func_code = (uint16_t)( (buffer[1] & 0x0F) << 8) | (buffer[2]);
  if(flags) *flags = buffer[3];
  return 0;
}