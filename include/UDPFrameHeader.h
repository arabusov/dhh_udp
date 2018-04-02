#pragma once
#include <stdint.h>
#define MAGIC 0xff00
#define START_OF_FRAME 0x02
#define END_OF_FRAME 0x01

struct udp_header
{
  uint16_t  magic;
  uint16_t  chunk_id;
  uint8_t   dhh_frame_id;
  uint8_t   chunk_flag;
}
