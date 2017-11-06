#include<stdio.h> //printf
#include<string.h> //memset
#include<stdlib.h> //exit(0);
#include<arpa/inet.h>
#include<sys/socket.h>
#include <sys/types.h>
#include<sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#define MEMDUMP 52000 
#define CHUNK_SIZE 1500
#define SERVER "127.0.0.1"
#define PORT 6000   //The port on which to send data

 
void die(char *s)
{
    perror(s);
    exit(1);
}
int next_chunk_id (int chunk_id)
{
  return chunk_id+1;
}
int main(void)
{
    struct sockaddr_in si_other;
    int s, i, slen=sizeof(si_other);
    int chunk_id = 0;
    int first = 1;
    char *message=NULL;
    char *buffer;
    int event;
    int fd;
    //Random udp package stuff, store into file
    message=malloc (MEMDUMP);
    buffer = message;
    for (int i = 0; i<MEMDUMP; i++)
      message[i] = (char)(rand()&0xff);
     
    fd = open ("rand.pat", O_WRONLY|O_CREAT|O_TRUNC, 0666);
    write (fd, message, MEMDUMP);
    close (fd);

    //UDP stuff
    if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        die("socket");
    }
 
    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(PORT);
     
    if (inet_aton(SERVER , &si_other.sin_addr) == 0) 
    {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }
    event=0;
    while (1)
    {
    chunk_id=0;
    first =1;
    printf ("Event No. %d sent\r", event++);
    for (i=0; i<MEMDUMP/CHUNK_SIZE+1; i++)
    {
        unsigned int message_size=0;
        message = buffer + i*CHUNK_SIZE;
        if (i>MEMDUMP/CHUNK_SIZE-1)
          message_size=MEMDUMP % CHUNK_SIZE;
        else
          message_size = CHUNK_SIZE;
        //send the message
        if (first)
        {
          first = 0;
          if (sendto(s, message, message_size, 0 , (struct sockaddr *) &si_other, slen)==-1)
            die("sendto()");
        }
        else
        {
          *(message-4) =(unsigned char) 255; *(message-3)=(unsigned
          char)0x00;
          *(message-2)=(unsigned
          char)(chunk_id>>8);*(message-1)=(unsigned char)chunk_id%256;
          chunk_id = next_chunk_id (chunk_id);
          if (sendto(s, message-4, message_size+4 , 0 , (struct sockaddr *) &si_other, slen)==-1)
            die("sendto()");
        }
        //receive a reply and print it
        //clear the buffer by filling null, it might have previously received data
    }
} 
    close(s);
    free (buffer);
    return 0;
}
