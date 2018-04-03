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
#include "../include/UDPFrameHeader.h"

#include <gcrypt.h>
#define BUFLEN 9000  //Max length of buffer
#define PORT 6000   //The port on which to listen for incoming data
#define MAX_NCHUNK_PER_FRAME 100
#define VERBOSE_LEVEL 1000

bool no_exit_flag=true;
int fd;
int event_counter = 0;
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

struct cell table [MAX_DHH_ID+1][MAX_NCHUNK_PER_FRAME];
unsigned int factual_table_size[MAX_DHH_ID+1];
unsigned int factual_frame_size[MAX_DHH_ID+1];
void dump_buffer (char *buf, int recv_len)
{
  if (buf==NULL)
  {
    fprintf (stderr, "Internal error: empty buffer to dump\n");
    return;
  }
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
#if 0
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
#endif
bool parse_udp_header (char* buf, size_t buf_len, struct udp_header *
  parsed_header)
{
  if (buf_len < sizeof (struct udp_header))
    return false;
  *parsed_header= *((struct udp_header*) (buf));
  if (parsed_header->magic != MAGIC)
  {
    fprintf (stderr, "There is no magic\n");
    return false;
  }
  if (parsed_header->flag > 2)
    return false;
  return true;
}

void add_buf_in_table (char *buf, unsigned int buf_len)
{
  struct udp_header header;
  bool is_DHH_UDP_frame = parse_udp_header (buf, buf_len, &header);
  uint8_t dhh_id = header.dhh_frame_id;
  if (factual_table_size[dhh_id] >= MAX_NCHUNK_PER_FRAME-1)
  {
    free (buf);
    fprintf (stderr, "Too many chunks. Max N_chunks is %d\n", MAX_NCHUNK_PER_FRAME);
  }
  else
  {
    if (is_DHH_UDP_frame)
    {
      if (dhh_id != header.dhh_frame_id)
      {
        fprintf (stderr, "Internal error, dhh_frame_id!=header.dhh_frame_id\n");
        return;
      }
      table[dhh_id][factual_table_size[dhh_id]].chunk_id =
        ntohs(header.chunk_id);
      table[dhh_id][factual_table_size[dhh_id]].pointer = buf;
      table[dhh_id][factual_table_size[dhh_id]].dhh_frame_id = header.dhh_frame_id;
      table[dhh_id][factual_table_size[dhh_id]].size = buf_len;
      table[dhh_id][factual_table_size[dhh_id]].flag = header.flag;
      factual_table_size[dhh_id]++;
      factual_frame_size[dhh_id] += buf_len-sizeof(struct udp_header);
    }
    else
    {
      fprintf (stderr, "Unknow UDP frame\n");
      if (dump_if_error)
      {
        dump_buffer (buf, buf_len);
      }
      return;
    }
  }
}

void init_table (uint8_t j)
{
  for (int i=0; i< MAX_NCHUNK_PER_FRAME; i++)
  {
    table[j][i].chunk_id = 0;
    table[j][i].pointer = NULL;
    table[j][i].dhh_frame_id = 0;
    table[j][i].flag= 0x4;
    table[j][i].size = 0;
  }
  factual_table_size[j] = 0;
  factual_frame_size[j] = 0;
}

void init_tables ()
{
  for (int j=0; j<MAX_DHH_ID+1; j++)
  {
    init_table (j);
  }
}

bool store_table (int fd, bool perform_dump, uint8_t dhh_id)
{
  bool chunk_error = false, no_sof=false, no_eof=false;
  qsort (table[dhh_id], factual_table_size[dhh_id], sizeof (struct cell), compare_cells);
  for (int i = 0; i<factual_table_size[dhh_id]; i++)
  {
    if(table[dhh_id][i].chunk_id != i)
    {
      printf ("Chunk order error: index = %d, id = %d in DHH frame No. %d\n", i,
        table[dhh_id][i].chunk_id, event_counter);
      chunk_error = true;
    }
    if ((i>0) && (i<factual_table_size[dhh_id]-1))
    {
      if (table[dhh_id][i].dhh_frame_id != dhh_id)
      {
        fprintf (stderr, "Internal error: dhh_id != table[dhh_id].dhh_id\n");
        chunk_error = true;
      }
    }
  }
  if (table[dhh_id][0].flag != START_OF_FRAME)
  {
    printf ("No start of frame UDP chunk in DHH frame No. %d, DHH frame ID %d, chunck id %d\n",
      event_counter, dhh_id, table[dhh_id][0].chunk_id);
    no_sof = true;
    chunk_error = true;
  }
  if (table[dhh_id][factual_table_size[dhh_id]-1].flag != END_OF_FRAME)
  {
    printf ("No end of frame UDP chunk in DHH frame No. %d, DHH frame ID %d, chunck id %d\n",
      event_counter, dhh_id, table[dhh_id][factual_table_size[dhh_id]-1].chunk_id);
    no_eof = true;
    chunk_error = true;
  }
  if (fd!=-1)
  {
    //store header
    char dhq_header [16];
      //magic
    dhq_header[0]=0xca; dhq_header[1] = 0xfe; dhq_header[2]=0xba;
    dhq_header[3]=0xbe;
      //size
    dhq_header[4]=(0xff000000 & factual_frame_size[dhh_id]) >> 24;
    dhq_header[5]=(0x00ff0000 & factual_frame_size[dhh_id]) >> 16;
    dhq_header[6]=(0x0000ff00 & factual_frame_size[dhh_id]) >> 8;
    dhq_header[7]=(0x000000ff & factual_frame_size[dhh_id]);
      //reserved
    dhq_header[8]=0x00; dhq_header[9] = 0x00; dhq_header[10]=0x00;
    dhq_header[11]=0x00; dhq_header[12] = 0x00; dhq_header[13]=0x00;
    dhq_header[14]=0x00;
    dhq_header[15]=0x00;
    //frame size %4 != 0 flag
    if (factual_frame_size[dhh_id]%4 != 0)
    {
        printf ("Byte conseq. shift in frame No. %d\n",event_counter);
        if (perform_dump)
        {
            for (int i=0; i<factual_table_size[dhh_id];i++)
            dump_buffer (table[dhh_id][i].pointer,table[dhh_id][i].size);
        }
        dhq_header[15]+=0x02;
    }
      //chunk error flag
    if (chunk_error)
      dhq_header[15]+=0x01;
    //sof missing
    if (no_sof)
      dhq_header[15]+=0x04;
    //eof missing
    if (no_eof)
      dhq_header[15]+=0x08;
    //res
    ssize_t written;
    written=write (fd, dhq_header, 16);
    if (written != 16)
      fprintf (stderr, "Written %ld, should be %d in DHH frame No. %d\n", written,
        16, event_counter);
#if 0
    //store the first frame
    written=write (fd, table[dhh_id][0].pointer, table[0].size);
    if (written != table[0].size)
      fprintf (stderr, "Written %d, should be %d in DHH frame No. %d\n", written,
        table[0].size, event_counter);
    //store other frames
#endif
    for (int i = 0; i<factual_table_size[dhh_id]; i++)
    {
      written=write (fd, table[dhh_id][i].pointer+sizeof(struct udp_header),
        table[dhh_id][i].size-sizeof(struct udp_header));
        if (written != table[dhh_id][i].size-sizeof(struct udp_header))
        {
          fprintf (stderr, "Written %ld, should be %ld in DHH frame No. %d, chunk id no %d\n",
            written, table[dhh_id][i].size-sizeof(struct udp_header), event_counter, i);
        }
    }
  }
  if (event_counter%VERBOSE_LEVEL == 0)
    printf ("DHH Frame No.: %d\n", event_counter);
  event_counter++;
  return chunk_error;
}

void free_buffer (uint8_t dhh_id)
{
  for (int i=0; i< factual_table_size[dhh_id]; i++)
  {
    table[dhh_id][i].chunk_id = 0;
    table[dhh_id][i].dhh_frame_id = 0;
    if (table[dhh_id][i].pointer)
      free (table[dhh_id][i].pointer);
    table[dhh_id][i].size = 0;
  }
  factual_table_size [dhh_id] = 0;
  factual_frame_size [dhh_id] = 0;
}

void free_buffers ()
{
  for (int i=0; i< MAX_DHH_ID+1; i++)
  {
    free_buffer (i);
  }
}

void terminate (int signal)
{
  printf ("Signal %d\n", signal);
  no_exit_flag = false;
  for (int i=0; i<MAX_DHH_ID+1; i++)
  {
    if (factual_table_size[i] != 0)
    {
      if (table[i][factual_table_size[i]-1].flag==END_OF_FRAME)
      {
        store_table (fd, dump_if_error, i);
        free_buffer (i);
      }
    }
  }
  close (fd);
  printf ("\n");
  exit (0);
}

void parse_args (int argc, char **argv, unsigned int * udp_port, bool *
  dump_every_event, bool * dump_if_error, char **output_file_name)
{
  int key;
    while ((key = getopt (argc, argv, "hdepm:")) != -1)
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
        case 'm':
          *md5_hash = atoi (optarg);
          break;
        case '?':
          if (optopt == 'p')
            fprintf (stderr, "Option -p requires an argument.\n");
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
    for (int i=0; i<MAX_DHH_ID+1;i++)
      init_table(i);
    fd = open ("/dev/null", O_WRONLY);
    if (raw_file_name)
      fd = open (raw_file_name, O_WRONLY|O_CREAT|O_TRUNC,0666);
    if (fd == -1)
    {
      printf ("Can't create file %s\n", raw_file_name);
    }
    struct udp_header header;
    while(no_exit_flag)
    {
         
        //try to receive some data, this is a blocking call

        create_buffer (&buf);
        if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) == -1)
        {
            die("recvfrom()");
        }
        bool is_DHH_UDP_frame = parse_udp_header (buf, recv_len, &header);
        add_buf_in_table (buf, recv_len);
        uint8_t dhh_id = header.dhh_frame_id;
        if ((header.flag == END_OF_FRAME) && (factual_table_size[dhh_id]!=0))
        {
          store_table (fd, dump_if_error, dhh_id);
        }
         
        //print details of the client/peer and the data received
        if (dump_flag)
        {
          printf("Received packet from %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
          printf ("Received: %d Bytes\n",recv_len);
          dump_buffer (buf, recv_len);
        }
        if ((header.flag == END_OF_FRAME) && (factual_table_size[dhh_id]!=0))
        {
          free_buffer(dhh_id);
        }
    } 
    store_table (fd, dump_if_error, header.dhh_frame_id);
    return 0;
}
