#pragma once
#include <stdint.h>
#define MAGIC 0x00ff
#define START_OF_FRAME 0x02
#define END_OF_FRAME 0x01
#define MAX_DHH_ID 0xff

struct udp_header
{
  uint16_t  magic;
  uint16_t  chunk_id;
  uint8_t   dhh_frame_id;
  uint8_t   flag;
};

