#include <stdio.h> //printf
#include <string.h> //memset
#include <stdlib.h> //exit(0);
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>
#include "../include/UDPFrameHeader.h"
#define MEMDUMP 52000 
#define CHUNK_SIZE 1450
#define SERVER "127.0.0.1"
#define PORT 6000   //The port on which to send data

void die(char *s)
{
    perror(s);
    exit(1);
}
uint16_t next_chunk_id (int chunk_id)
{
  return chunk_id+1;
}

void delay ()
{
  usleep (10000);
}

int main(void)
{
    struct sockaddr_in si_other;
    int s, i, slen=sizeof(si_other);
    int first = 1;
    int last=0;
    char *message=NULL;
    char *buffer;
    int event;
    int fd;
    uint8_t dhh_id = (uint8_t)rand()&0xff;
    //Random udp package stuff, store into file
    message=malloc (MEMDUMP+sizeof(struct udp_header));
    buffer = message;
    for (int i = sizeof(struct udp_header); i<MEMDUMP+sizeof(struct udp_header); i++)
      message[i] = (char)(rand()&0xff);
     
    fd = open ("rand.pat", O_WRONLY|O_CREAT|O_TRUNC, 0666);
    write (fd, message+sizeof(struct udp_header), MEMDUMP);
    close (fd);

    //UDP stuff
    if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        die("socket");
    }
 
    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(PORT);
     
    if (inet_pton(AF_INET, SERVER , &si_other.sin_addr) == 0) 
    {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }
    event=0;
    //while (1)
    {
    uint16_t chunk_id = 0;
    struct udp_header header;
    header.magic=MAGIC;
    header.chunk_id=htons (chunk_id);
    header.dhh_frame_id = dhh_id;
    first =1;
    printf ("Frame No. %d sent\r", header.dhh_frame_id);
    for (i=0; i<MEMDUMP/CHUNK_SIZE+1; i++)
    {
        unsigned int message_size=0;
        message = buffer +sizeof(struct udp_header) + i*CHUNK_SIZE;
        if (i>MEMDUMP/CHUNK_SIZE-1)
        {
          message_size=MEMDUMP % CHUNK_SIZE;
          last = 1;
        }
        else
        {
          message_size = CHUNK_SIZE;
        }
        //send the message
        delay();
        if (first)
        {
          header.flag = START_OF_FRAME;
          first = 0;
        }
        else if (last)
        {
          header.flag = END_OF_FRAME;
          last=0;
        }
        else
        {
          header.flag = 0x00;
        }
        header.chunk_id  = htons (chunk_id);
        *(struct udp_header*)(message-sizeof(struct udp_header)) = header;
        if (sendto(s, message-sizeof(struct udp_header),
          message_size+sizeof(struct udp_header), 0, (struct sockaddr *) &si_other, slen)==-1)
            die("sendto()");
        chunk_id= next_chunk_id (chunk_id);
        //receive a reply and print it
        //clear the buffer by filling null, it might have previously received data
    }
      delay();
    } 
    close(s);
    free (buffer);
    return 0;
}
