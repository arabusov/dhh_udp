#include<stdio.h>
#include<stdbool.h>
#include<string.h>
#include<stdlib.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include <ctype.h>
#include <unistd.h>
#include <memory.h> 
#include <signal.h>
#include<fcntl.h>
#include <getopt.h>
#include <stdint.h> 
#include "include/UDPFrameHeader.h"
#define BUFLEN 9000  //Max length of buffer
#define PORT 6000   //The port on which to listen for incoming data
#define MAX_NCHUNK_PER_FRAME 100
#define VERBOSE_LEVEL 1000
bool no_exit_flag = true;
int fd;
int event_counter = 0;
uint8_t dhh_frame_id =0;
bool dump_if_error=false;
void die(char *s)
{
    perror(s);
    exit(1);
}

struct cell
{
  uint16_t chunk_id;
  char* pointer;
  size_t size;
  uint8_t dhh_frame_id;
  uint8_t flag;
};

int compare_cells (const void *c1, const void *c2)
{
  if ( ((struct cell *) c1)->chunk_id < ((struct cell *) c2)->chunk_id)
    return -1;
  if ( ((struct cell *) c1)->chunk_id == ((struct cell *) c2)->chunk_id)
    return 0;
  if ( ((struct cell *) c1)->chunk_id > ((struct cell *) c2)->chunk_id)
    return 1;
}

struct cell table [MAX_NCHUNK_PER_FRAME];
unsigned int factual_table_size = 0;
unsigned int factual_frame_size = 0;
void dump_buffer (char *buf, int recv_len)
{
  printf ("\nDump UDP buffer (%d bytes): \n", recv_len);
				for (int i=0; i < recv_len; i++)
				{
        	printf("%02X" , (unsigned char)buf[i]);
					if (i%36==35)
						printf ("\n");
					else
						if (i%4==3)
						  printf(" ");
				}
  printf ("\nEnd dump.\n");
}

void create_buffer (char **buf)
{
  *buf = (char*)malloc(BUFLEN*sizeof(char));
}

unsigned int get_chunk_id_old (char *buf, unsigned int buf_len)
{
  unsigned int chunk_id=0;
  //chunk flag
  if (buf_len >3)
    if (((unsigned char)buf[0]==0xff) && ((unsigned char)buf[1] == 0x00))
    {
      chunk_id = (((unsigned int) buf[2] )<< 8 )+ (unsigned int) buf[3];//why not? :) why yes!
    }
  return chunk_id;
}

bool is_chunk_old (char* buf, unsigned int buf_len)
{
    bool ret = false;
    if (buf_len>3)
        ret = (((unsigned char)buf[0]==0xff) && ((unsigned char)buf[1] == 0x00));
    return ret;
}

bool parse_udp_header (char* buf, size_t buf_len, struct udp_header *
  parsed_header)
{
  if (buf_len < 6)
    return false;
  *parsed_header= *((struct udp_header*) (buf));
  if (parsed_header->magic != MAGIC)
    return false;
  if (parsed_header->chunk_flag > 2)
    return false;
  return true;
}

void add_buf_in_table (char *buf, unsigned int buf_len)
{
  if (factual_table_size >= MAX_NCHUNK_PER_FRAME-1)
  {
    free (buf);
    fprintf (stderr, "Too many chunks. Max N_chunks is %d\n", MAX_NCHUNK_PER_FRAME);
  }
  else
  {
    struct udp_header header;
    bool res = parse_udp_header (buf, buf_len, header);
    if (res)
    {
      if (dhh_frame_id != header.dhh_frame_id)
      {
        fprintf (stderr, "Internal error, dhh_frame_id!=header.dhh_frame_id\n");
        return;
      }
      table[factual_table_size].chunk_id = header.chunk_id;
      table[factual_table_size].pointer = buf;
      table[factual_table_size].dhh_frame_id = header.dhh_frame_id;
      table[factual_table_size].size = buf_len;
      table[factual_table_size].flag = header.chunk_flag;
      factual_table_size++;
      factual_frame_size += buf_len-sizeof(struct udp_header);
    }
    else
    {
    }
  }
}

void init_table ()
{
  for (int i=0; i< MAX_NCHUNK_PER_FRAME; i++)
  {
    table[i].chunk_id = 0;
    table[i].pointer = NULL;
    table[i].size = 0;
  }
  factual_table_size = 0;
  factual_frame_size = 0;
}

bool store_table (int fd, bool perform_dump)
{
  bool chunk_error = false;
  qsort (table, factual_table_size, sizeof (struct cell), compare_cells);
  for (int i = 0; i<factual_table_size; i++)
  {
    if(table[i].chunk_id != i)
    {
      printf ("Chunk order error: index = %d, id = %d in DHH frame No. %d\n", i,
        table[i].chunk_id, event_counter);
      chunk_error = true;
    }
  }
  if (fd!=-1)
  {
    //store header
    char dhq_header [16];
      //magic
    dhq_header[0]=0xca; dhq_header[1] = 0xfe; dhq_header[2]=0xba;
    dhq_header[3]=0xbe;
      //size
    dhq_header[4]=(0xff000000 & factual_frame_size) >> 24;
    dhq_header[5]=(0x00ff0000 & factual_frame_size) >> 16;
    dhq_header[6]=(0x0000ff00 & factual_frame_size) >> 8;
    dhq_header[7]=(0x000000ff & factual_frame_size);
      //reserved
    dhq_header[8]=0x00; dhq_header[9] = 0x00; dhq_header[10]=0x00;
    dhq_header[11]=0x00; dhq_header[12] = 0x00; dhq_header[13]=0x00;
    dhq_header[14]=0x00;
    dhq_header[15]=0x00;
    //frame size %4 != 0 flag
    if (factual_frame_size%4 != 0)
    {
        printf ("Byte conseq. shift in frame No. %d\n",event_counter);
        if (perform_dump)
        {
            for (int i=0; i<factual_table_size;i++)
            dump_buffer (table[i].pointer,table[i].size);
        }
        dhq_header[15]+=0x02;
    }
      //chunk error flag
    if (chunk_error)
      dhq_header[15]+=0x01;
    //res
    ssize_t written;
    written=write (fd, dhq_header, 16);
    if (written != 16)
      fprintf (stderr, "Written %d, should be %d in DHH frame No. %d\n", written,
        16, event_counter);
    //store the first frame
    written=write (fd, table[0].pointer, table[0].size);
    if (written != table[0].size)
      fprintf (stderr, "Written %d, should be %d in DHH frame No. %d\n", written,
        table[0].size, event_counter);
    //store other frames
    for (int i = 1; i<factual_table_size; i++)
    {
      written=write (fd, table[i].pointer+4*sizeof(char), table[i].size-4*sizeof(char));
        if (written != table[i].size-4*sizeof(char))
        fprintf (stderr, "Written %d, should be %d in DHH frame No. %d, chunk id no %d\n",
                written,
            table[i].size-4*sizeof(char), event_counter, i);
    }
  }
  if (event_counter%VERBOSE_LEVEL == 0)
    printf ("DHH Frame No.: %d\n", event_counter);
  event_counter++;
  return chunk_error;
}


void free_buffers ()
{
  for (int i=0; i< factual_table_size; i++)
  {
    table[i].chunk_id = 0;
    if (table[i].pointer)
      free (table[i].pointer);
    table[i].size = 0;
  }
  factual_table_size = 0;
  factual_frame_size = 0;
}

void terminate (int signal)
{
  printf ("Signal %d\n", signal);
  no_exit_flag = false;
    if (factual_table_size != 0)
    {
      store_table (fd, dump_if_error);
      free_buffers ();
    }
//    close(s);
    close (fd);
    printf ("\n");
    exit (0);
}

void parse_args (int argc, char **argv, unsigned int * udp_port, bool *
  dump_every_event, bool * dump_if_error, char **output_file_name)
{
  int key;
    while ((key = getopt (argc, argv, "hdep:")) != -1)
    {
      switch (key)
      {
        case 'h':
          printf ("Receivs UDP packages, sort chunks and stores with ONSEN\
 headers\n");
          printf ("Usage: udp_receiver [FLAGS] [OUTPUT_RAW_FILE_NAME]\n");
          printf ("Flags:\n\t-d -- dump UDP packages\n\t-p [PORT] -- UDP\
 port, default port is %d\n\t-e -- dump UDP packages if error\n", PORT);
          exit (0);
        case 'd':
          *dump_every_event = true;
          break;
        case 'e':
          *dump_if_error = true;
          break;
        case 'p':
          *udp_port = atoi (optarg);
          break;
        case '?':
          if (optopt == 'p')
            fprintf (stderr, "Option -%p requires an argument.\n", optopt);
          else if (isprint (optopt))
            fprintf (stderr, "Unknown option `-%c'\n", optopt);
          else
            fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
          exit (1);
      }
    }
    if (optind < argc)
      *output_file_name = argv[optind];
    else
      *output_file_name = NULL;
}

int main(int argc, char **argv)
{
    struct sockaddr_in si_me, si_other;
     
    int i, slen = sizeof(si_other) , recv_len;
int s;//socket
    unsigned int udp_port=PORT;
    char *buf=NULL;
    bool dump_flag=false;
    char *raw_file_name=NULL;
    //parse arguments
    parse_args (argc, argv, &udp_port, &dump_flag, &dump_if_error, &raw_file_name);
    if (dump_flag)
      printf ("Will dump UDP packages.\n");
    else
      printf ("Won't dump UDP packages.\n");
    printf ("UDP Port: %d\n", udp_port);
    if (raw_file_name)
      printf ("Output RAW file: %s\n", raw_file_name);
    else
      printf ("Output RAW file doesn't set.\n");
    //create a UDP socket
    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        die("socket");
    }
     
    // zero out the structure
    memset((char *) &si_me, 0, sizeof(si_me));
     
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(udp_port);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
     
    //bind socket to port
    if( bind(s , (struct sockaddr*)&si_me, sizeof(si_me) ) == -1)
    {
        die("bind");
    }
    // catch sigterm
    signal (SIGINT, terminate);
    //keep listening for data
    printf("Waiting for UDP data, port %d...\n",udp_port);
    fflush(stdout);
    init_table();
    fd = open ("/dev/null", O_WRONLY);
    if (raw_file_name)
      fd = open (raw_file_name, O_WRONLY|O_CREAT|O_TRUNC,0666);
    if (fd == -1)
    {
      printf ("Can't create file %s\n", raw_file_name);
    }
    while(no_exit_flag)
    {
         
        //try to receive some data, this is a blocking call

        create_buffer (&buf);
        if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) == -1)
        {
            die("recvfrom()");
        }
        if ((! is_chunk  (buf, recv_len)) && (factual_table_size != 0))
        {
          store_table (fd, dump_if_error);
          free_buffers();
        }
        add_buf_in_table (buf, recv_len);
         
        //print details of the client/peer and the data received
        if (dump_flag)
        {
          printf("Received packet from %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
          printf ("Received: %d Bytes\n",recv_len);
          dump_buffer (buf, recv_len);
        }
    } 
    store_table (fd, dump_if_error);
    return 0;
}
