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
#define BUFLEN 65536  //Max length of buffer
#define PORT 6000   //The port on which to listen for incoming data
#define MAX_NCHUNK_PER_FRAME 100
 
bool no_exit_flag = true;
int fd;
int event_counter = 0;
void die(char *s)
{
    perror(s);
    exit(1);
}

struct cell
{
  unsigned int chunk_id;
  char* pointer;
  size_t size;
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

unsigned int get_chunk_id (char *buf, unsigned int buf_len)
{
  unsigned int chunk_id=0;
  //chunk flag
  if (buf_len >3)
    if (((unsigned char)buf[0]==0xff) && ((unsigned char)buf[1] == 0x00))
    {
      chunk_id = (((unsigned int) buf[2] )<< 8 )+ (unsigned int) buf[3]+1;//why not? :) why yes!
    }
  return chunk_id;
}

void add_buf_in_table (char *buf, unsigned int buf_len)
{
  if (factual_table_size >= N_CHUNK_PER_FRAME-1)
  {
    free (buf);
  }
  else
  {
    table[factual_table_size].chunk_id = get_chunk_id (buf, buf_len);
    table[factual_table_size].pointer = buf;
    table[factual_table_size].size = buf_len;
    factual_table_size++;
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
}

bool store_table (int fd)
{
  bool chunk_error = false;
  qsort (table, factual_table_size, sizeof (struct cell), compare_cells);
  for (int i = 0; i<factual_table_size; i++)
  {
    if(table[i].chunk_id != i)
    {
      fprintf (stderr, "Chunk order error: index = %d, id = %d in event No. %d\n", i,
        table[i].chunk_id, evenc_counter);
      chunk_error = true;
    }
  }
  if (fd!=-1)
  {
    write (fd, table[0].pointer, table[0].size);
    for (int i = 1; i<factual_table_size; i++)
    {
      write (fd, table[i].pointer+4, table[i].size-4);
    }
  }
  printf ("Event No.: %d, chunk error: %d\r", event_counter, chunk_error);
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
}

void terminate (int signal)
{
  printf ("Signal %d\n", signal);
  no_exit_flag = false;
    if (factual_table_size != 0)
    {
      store_table (fd);
      free_buffers ();
    }
//    close(s);
    close (fd);
    printf ("\n");
    exit (0);
}

void parse_args (int argc, char **argv, unsigned int * udp_port, bool *
  dump_every_event, char **output_file_name)
{
  int key;
    while ((key = getopt (argc, argv, "hdp:")) != -1)
    {
      switch (key)
      {
        case 'h':
          printf ("Usage: udp_receiver [FLAGS] [OUTPUT_RAW_FILE_NAME]\n");
          printf ("Flags:\n\t-d -- dump UDP packages\n\t-p [PORT] -- UDP\
 port, default port is %d\n", PORT);
          exit (0);
        case 'd':
          *dump_every_event = true;
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
    parse_args (argc, argv, &udp_port, &dump_flag, &raw_file_name);
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
//    fd = open ("result.dat", O_WRONLY|O_CREAT|O_TRUNC,0666);
    fd = open ("/dev/null", O_WRONLY);
    while(no_exit_flag)
    {
         
        //try to receive some data, this is a blocking call

        create_buffer (&buf);
        if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) == -1)
        {
            die("recvfrom()");
        }
        if ((get_chunk_id (buf, recv_len) == 0) && (factual_table_size != 0))
        {
          store_table (fd);
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
    return 0;
}
