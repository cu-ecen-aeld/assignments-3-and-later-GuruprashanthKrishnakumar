/*
*   file:      aesdsocket.c
*   brief:     Implements socket functionality. Listens to port 9000 and stores any packet received in the file /var/aesdsocketdata and sends it back over the same port.
*              Packets are assumed to be separated with the \n character.
*              Can optionally be executed as daemon with '-d' command line argument.
*   author:    Guruprashanth Krishnakumar, gukr5411@colorado.edu
*   date:      10/01/2022
*   refs:      https://beej.us/guide/bgnet/html/, lecture slides of ECEN 5713 - Advanced Embedded Software Dev.
*/
/*
*   HEADER FILES
*/
#include <sys/types.h>
#include <sys/socket.h>
#include "queue.h"
#include <pthread.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <time.h>
#include "aesd_ioctl.h"
/*
*   MACROS
*/
#define BUF_SIZE_UNIT           (1024)
#define USE_AESD_CHAR_DEVICE

#ifdef  USE_AESD_CHAR_DEVICE
    #define LOG_FILE                ("/dev/aesdchar")
    #define IOCTL_COMMAND           ("")
#else
    #define LOG_FILE                ("/var/tmp/aesdsocketdata")
    #define CIRCULAR_BUF_DEPTH      (8)
#endif
/*
*   GLOBALS
*/
typedef enum
{
    Accept_Connections,
    Join_threads,
}main_thread_states_t;
typedef enum
{
    Receive_From_Socket,
    Parse_data,
}worker_threads_states_t;
struct worker_thread_s
{
    pthread_t thread_id;
    bool thread_completed;
    worker_threads_states_t curr_state;
    int socket_file_descriptor;
    char ip_addr[INET6_ADDRSTRLEN];
    TAILQ_ENTRY(worker_thread_s) entries;
};
typedef struct worker_thread_s worker_thread_t;
typedef TAILQ_HEAD(head_s, worker_thread_s) head_t;
typedef struct
{
    bool signal_caught;
    bool free_address_info;
    bool free_socket_descriptor;
    struct addrinfo *host_addr_info;
    int socket_descriptor;
    int connection_count;
#ifndef USE_AESD_CHAR_DEVICE
    bool disarm_alarm;
    pthread_mutex_t mutex;
    timer_t timer_1;
    struct itimerspec itime;
#endif
}socket_state_t;
socket_state_t socket_state;

#ifdef USE_AESD_CHAR_DEVICE
typedef struct
{
	const char *command;
} command_table_t;

static const command_table_t commands[] = {
    {"AESDCHAR_IOCSEEKTO:"}
};
#else
typedef struct
{
    char time_string[100];
}circular_buf_data_t;
typedef struct
{
    uint32_t wptr;
    uint32_t rptr;
    bool queue_empty;
    bool queue_full;
    circular_buf_data_t time_buf[CIRCULAR_BUF_DEPTH];
}circular_buf_metadata_t;
circular_buf_metadata_t circular_buf;
#endif
/*
*   STATIC FUNCTION PROTOTYPES
*/
/*
*   Checks the state of booleans in the socket_state structure and performs neccesarry cleanups like 
*   closing fds, freeing memory etc.,
*
*   Args:
*       None
*   Params:
*       None
*/
static void perform_cleanup();
#ifndef USE_AESD_CHAR_DEVICE 
/*
*   Thread-per-connection. Reads off a socket descriptor until recv
*   returns 0 and dumps when \n is encountered.
*
*   Args:
*       data - buffer to store dequeued timestamp
*   Params:
*       success/failure
*/
static void* server_thread(void* thread_param);

/*
*   Initializes circular buffer used for storing timestamps
*
*   Args:
*       None
*   Params:
*       None
*/
static void initialize_circular_buf();
/*
*   Create an interval timer and arm it to expire every 10s.
*
*   Args:
*       None
*   Params:
*       None
*/
static int create_and_arm_timer();

/*
*   Disarm the timer and close it.
*
*   Args:
*       none
*   Params:
*       none 
*/
static void disarm_and_destroy_timer();

/*
*   Signal handler for SIGINT and SIGTERM. Enqueue timestamp into circular buffer.
*
*   Args:
*       None
*   Params:
*       None
*/
static void alarmhandler();

/*
*   Enqueues timestamp into circular buffer.
*
*   Args:
*       data - data to be enqueued
*   Params:
*       success/failure
*/
static int enqueue_data_into_circ_buf(char* data);
/*
*   Dequeues timestamp from circular buffer.
*
*   Args:
*       data - buffer to store dequeued timestamp
*   Params:
*       success/failure
*/
static int dequeue_data_from_circ_buf(char* buf); 

/*
*   Return the next pointer of read/write pointer passed while handling wraparound
*
*   Args:
*       ptr - pointer for which nextptr needs to be calculated
*   Params:
*       nextptr
*/
static uint32_t nextPtr(uint32_t ptr);
#else
static void* aesd_char_thread(void* thread_param);
#endif

/*
*   Setup signal handler for the signal number passed 
*
*   Args:
*       signo - signal number for which handler needs to be set up for 
*   Params:
*       0 if successful, -1 if failed 
*/
static int setup_signal(int signo);

/*
*   Signal handler for SIGINT and SIGTERM. If any open connection is on-going on the socket,
*   it sets a flag that a signal was caught. Otherwise, performs neccessarry cleanup and exits.
*
*   Args:
*       None
*   Params:
*       None
*/
static void sighandler();

/*
*   Returns the IP address present in the socket address data structure passed.
*
*   Args:
*       sockaddr    -   socket address data structure
*   Params:
*       IPv4 or IPv6 address contained in the socket address data structure
*/
static void *get_in_addr(struct sockaddr *sa);
/*
*   Dumps the content passed to a file.
*
*   Args:
*       fd    -   handle to the file to write into
*       string  -   Data to write to file
*       write_len   -   Number of bytes contained in string.
*   Params:
*       Success or failure
*/
static int dump_content(int fd, char* string,int write_len);
/*
*   Reads from a file and echoes it back across the socket.
*
*   Args:
*       fd    -   handle to the file to read
*       read_len   -   Number of bytes contained in file.
*   Params:
*       Success or failure
*/
static int echo_file_socket(int fd, int socket_fd);
/*
*   Initializes the socket state structure to a known initial state.
*
*   Args:
*       None
*   Params:
*       None
*/
static void initialize_socket_state();

/*
*   FUNCTION DEFINITIONS
*/

#ifndef USE_AESD_CHAR_DEVICE
static void initialize_circular_buf()
{
    circular_buf.wptr = 0;
    circular_buf.rptr = 0;
    circular_buf.queue_full = false;
    circular_buf.queue_empty = true;
}

static uint32_t nextPtr(uint32_t ptr) {
  // Student edit:
  // Create this function
  return ((ptr+1)&(CIRCULAR_BUF_DEPTH - 1));
} // nextPtr()


/*
*    Note on Circular buffer: 
*                The circular buffer is not reentrant. The enqueue and dequeue functions do not 
*                modify the same variables and hence calls to enqueue from within dequeue will not
*                cause race conditions. However, a call to enqueue from within enqueue will cause over-writes (similarly for dequeue it might cause skips)
*                This circular buffer implementation works as long as there is one producer and one consumer (as is the case with this program, alarmhandler
*                is the producer and the whatever writes to the file is the consumer)
*/
static int enqueue_data_into_circ_buf(char* data)
{
  if(circular_buf.queue_full)
  {
    return -1;
  }
  strncpy(circular_buf.time_buf[circular_buf.wptr].time_string,data,80);
  circular_buf.wptr = nextPtr(circular_buf.wptr);
  circular_buf.queue_empty = false;
  if(circular_buf.wptr == circular_buf.rptr)
  {
    circular_buf.queue_full = true;
  }
  return 0;
}
static int dequeue_data_from_circ_buf(char* buf)
{
  if(circular_buf.queue_empty)
  {
    return -1;
  }
  strncpy(buf,circular_buf.time_buf[circular_buf.rptr].time_string,80);
  circular_buf.rptr = nextPtr(circular_buf.rptr);
  circular_buf.queue_full = false;
  if(circular_buf.wptr == circular_buf.rptr)
  {
    circular_buf.queue_empty = true;
  }
  return 0;
}


static int create_and_arm_timer()
{
    int flags = 0;
    int status = 0;
    status = timer_create(CLOCK_REALTIME, NULL, &socket_state.timer_1);
    if(status == -1)
    {
        syslog(LOG_ERR,"Create timer: %s",strerror(errno));
        return -1;
    }
    socket_state.itime.it_interval.tv_sec = 10;
    socket_state.itime.it_interval.tv_nsec = 0;
    socket_state.itime.it_value.tv_sec = 10;
    socket_state.itime.it_value.tv_nsec = 0;
    status = timer_settime(socket_state.timer_1, flags, &socket_state.itime,NULL);
    if(status == -1)
    {
        syslog(LOG_ERR,"Set timer %s",strerror(errno));
        return -1;
    }
    return 0;
}
static void disarm_and_destroy_timer()
{
    int flags = 0;
    socket_state.itime.it_interval.tv_sec = 0;
    socket_state.itime.it_interval.tv_nsec = 0;
    socket_state.itime.it_value.tv_sec = 0;
    socket_state.itime.it_value.tv_nsec = 0;
    timer_settime(socket_state.timer_1, flags, &socket_state.itime,NULL);
    timer_delete(socket_state.timer_1);
}

/*
*   Functions time() and localtime_r are reentrant and hence are safe to call from signal handler.
*   strftime() has not been called since it is not reentrant.
*   Limitations of circular buffer have been documented above.
*/
static void alarmhandler()
{
    time_t rawtime;
    struct tm info;
    char buffer[80];
    time( &rawtime );
    localtime_r( &rawtime,&info );
    sprintf(buffer,"timestamp: %d/%02d/%02d - %02d:%02d:%02d\n",(info.tm_year + 1900),(info.tm_mon + 1),info.tm_mday,info.tm_hour,info.tm_min,info.tm_sec);
    enqueue_data_into_circ_buf(buffer);
    
}
#endif

static int setup_signal(int signo)
{
    struct sigaction action;
    if(signo == SIGINT || signo == SIGTERM)
    {
        action.sa_handler = sighandler;
    }
    #ifndef USE_AESD_CHAR_DEVICE
    else if(signo == SIGALRM)
    {
        action.sa_handler = alarmhandler;
    }
    #endif
    action.sa_flags = 0;
    sigset_t empty;
    if(sigemptyset(&empty) == -1)
    {
        syslog(LOG_ERR, "Could not set up empty signal set: %s.", strerror(errno));
        return -1; 
    }
    action.sa_mask = empty;
    if(sigaction(signo, &action, NULL) == -1)
    {
        syslog(LOG_ERR, "Could not set up handle for signal: %s.", strerror(errno));
        return -1;         
    }
    return 0;
}

static void initialize_socket_state()
{
    socket_state.free_address_info = false;
    socket_state.free_socket_descriptor = false;
    socket_state.signal_caught = false;
    socket_state.host_addr_info = NULL;
    socket_state.connection_count = 0;
    #ifndef USE_AESD_CHAR_DEVICE
    socket_state.disarm_alarm = false;
    pthread_mutex_init(&socket_state.mutex, NULL);
    #endif
}
static void perform_cleanup()
{
    if(socket_state.host_addr_info && socket_state.free_address_info)
    {
        freeaddrinfo(socket_state.host_addr_info);
    }
    if(socket_state.free_address_info)
    {
        close(socket_state.socket_descriptor);
    }
    #ifndef USE_AESD_CHAR_DEVICE
    if(socket_state.disarm_alarm)
    {
        disarm_and_destroy_timer();
    }
    pthread_mutex_destroy(&socket_state.mutex);
    #endif
    //Disarm Alarm
    closelog();
}
static void shutdown_function()
{
    printf("\nCaught Signal. Exiting\n");
    perform_cleanup();
    #ifndef USE_AESD_CHAR_DEVICE
    printf("Deleting file\n");
    unlink("/var/tmp/aesdsocketdata");
    #endif
    exit(1);
}
static void sighandler()
{
    socket_state.signal_caught = true;
}

static void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
static int dump_content(int fd, char* string,int write_len)
{
    ssize_t ret; 
    while(write_len!=0)
    {
        ret = write(fd,string,write_len);
        if(ret == 0)
        {
            break;
        } 
        if(ret == -1)
        {
            if(errno == EINTR)
            {
                continue;
            }
            //printf("Write len %d\n",write_len);
            syslog(LOG_ERR,"Write: %s",strerror(errno));
            return -1;
        }
        write_len -= ret;
        string += ret;
    }
    return 0;
}
static int echo_file_socket(int fd, int socket_fd)
{
    ssize_t ret; 
    char write_str[BUF_SIZE_UNIT];
    while(1)
    {
        memset(write_str,0,sizeof(write_str));
        ret = read(fd,write_str,sizeof(write_str));
        if(ret == 0)
        {
            break;
        } 
        if(ret == -1)
        {
            if(errno == EINTR)
            {
                continue;
            }
            //printf("Read Len %d\n",read_len);
            syslog(LOG_ERR,"Read: %s",strerror(errno));
            return -1;
        }
        int num_bytes_to_send = ret;
        int num_bytes_sent = 0;
        int str_index = 0;
        while(num_bytes_to_send>0)
        {
            num_bytes_sent = send(socket_fd,&write_str[str_index],num_bytes_to_send,0);
            if(num_bytes_sent == -1)
            {
                syslog(LOG_ERR,"Send: %s",strerror(errno));
                return -1;
            }
            num_bytes_to_send -= num_bytes_sent;
            str_index += num_bytes_sent;
        }
    }
    return 0;
}
#ifndef USE_AESD_CHAR_DEVICE
static void* server_thread(void* thread_param)
{
    worker_thread_t *thread_params = (worker_thread_t *)thread_param;
    int file_descriptor,num_bytes_read = 0,start_ptr = 0,num_bytes_to_read=0,buf_len=0,buf_cap=0,status=0;
    char *ptr,*buf;
    while(1)
    {
        switch(thread_params->curr_state)
        {
            case Receive_From_Socket:
                if(buf_cap == buf_len)
                {
                    if(buf_cap == 0)
                    {
                        buf = malloc(BUF_SIZE_UNIT);
                        if(!buf)
                        {
                            printf("Insufficient memory. Exiting\n");
                            //socket_file_descriptor needs to be freed.
                            //completion boolean has to be set
                            goto free_socket_fd;
                        }
                    }
                    else
                    {
                        int new_len = buf_cap + BUF_SIZE_UNIT; 
                        char *new_buf;   
                        new_buf = realloc(buf,new_len);     
                        if(!new_buf)
                        {
                            printf("Insufficient memory. Exiting\n");
                            free(buf);
                            //buf has to be freed
                            //socket_file_descriptor needs to be freed.
                            //completion boolean has to be set
                            goto free_mem;
                        }
                        buf = new_buf;           
                    }
                    buf_cap += BUF_SIZE_UNIT;
                }
                num_bytes_read = 0;
                num_bytes_read = recv(thread_params->socket_file_descriptor,(buf+buf_len),(buf_cap - buf_len),0);
                if(num_bytes_read == -1)
                {
                    syslog(LOG_ERR,"Recv: %s",strerror(errno));
                    //buf has to be freed
                    //socket_file_descriptor needs to be freed.
                    //completion boolean has to be set
                    goto free_mem;
                }
                else if(num_bytes_read>0)
                {
                    thread_params->curr_state = Parse_data;
                }
                else if(num_bytes_read == 0)
                {
                    //buf has to be freed
                    //socket_file_descriptor needs to be freed.
                    //completion boolean has to be set
                    goto free_mem;
                }
                break;
            case Parse_data:
                num_bytes_to_read = ((buf_len - start_ptr) + num_bytes_read);
                //printf("Bytes read %d num_bytes_to_read %d\n",num_bytes_read,num_bytes_to_read);
                int temp_read_var = num_bytes_to_read;
                for(ptr = &buf[start_ptr];temp_read_var>0;ptr++,temp_read_var--)
                {
                    if(*ptr == '\n')
                    {
                        temp_read_var--;
                        status = pthread_mutex_lock(&socket_state.mutex);
                        if(status != 0)
                        {
                            syslog(LOG_ERR,"Mutex Lock: %s",strerror(errno));
                            //buf has to be freed
                            //socket_file_descriptor needs to be freed.
                            //completion boolean has to be set
                            goto free_mem;
                        }
                        file_descriptor = open(LOG_FILE,O_RDWR|O_CREAT|O_APPEND,S_IRWXU|S_IRWXG|S_IRWXO);
                        if(file_descriptor == -1)
                        {
                            syslog(LOG_ERR,"Open: %s",strerror(errno));
                            goto unlock_mutex;
                        }
                        //printf("Temp var read %d\n",temp_read_var);
                        int bytes_written_until_newline = (num_bytes_to_read - temp_read_var);
                        //printf("Total buffsize %d buf len %d bytes written until newline %d \n",buf_cap,buf_len,bytes_written_until_newline);
                        if(dump_content(file_descriptor,&buf[start_ptr],bytes_written_until_newline)==-1)
                        {
                            //file_descriptor needs to be closed
                            //mutex has to be unlocked
                            //buf has to be freed
                            //socket_file_descriptor needs to be freed.
                            //completion boolean has to be set
                            //perform cleanup and exit, but this time unlock first
                            goto close_file_descriptor;
                        }                        
                        lseek(file_descriptor, 0, SEEK_SET );
                        //printf("Total Bytes written to file %d\n",total_bytes_written_to_file);
                        if(echo_file_socket(file_descriptor,thread_params->socket_file_descriptor)==-1)
                        {
                            //file_descriptor needs to be closed
                            //mutex has to be unlocked
                            //buf has to be freed
                            //socket_file_descriptor needs to be freed.
                            //completion boolean has to be set
                            //perform cleanup and exit, but this time unlock first
                            goto close_file_descriptor;
                        }
                        char time_buf_string[80];
                    
                        if(dequeue_data_from_circ_buf(time_buf_string)==0)
                        {
                            if(dump_content(file_descriptor,time_buf_string,strlen(time_buf_string))!=-1)
                            {
                                printf("wrote from thread\n");
                                bytes_written_until_newline += strlen(time_buf_string);
                            }
                        }
                        start_ptr = bytes_written_until_newline;
                        close(file_descriptor);
                        status = pthread_mutex_unlock(&socket_state.mutex);
                        if(status != 0)
                        {
                            syslog(LOG_ERR,"Mutex Unlock: %s",strerror(errno));
                            goto unlock_mutex;
                        }
                        break;
                    }
                }
                buf_len += num_bytes_read;
                thread_params->curr_state = Receive_From_Socket;
                break;
        }
    }
    close_file_descriptor: close(file_descriptor);
    unlock_mutex: pthread_mutex_unlock(&socket_state.mutex);
    free_mem: free(buf);
    free_socket_fd: close(thread_params->socket_file_descriptor);
                    thread_params->thread_completed = true;
                    syslog(LOG_DEBUG,"Closed connection from %s",thread_params->ip_addr);
                    return 0;
}
#else
static void* aesd_char_thread(void* thread_param)
{
    worker_thread_t *thread_params = (worker_thread_t *)thread_param;
    int file_descriptor,num_bytes_read = 0,start_ptr = 0,num_bytes_to_read=0,buf_len=0,buf_cap=0;
    char *ptr,*buf;
    while(1)
    {
        switch(thread_params->curr_state)
        {
            case Receive_From_Socket:
                if(buf_cap == buf_len)
                {
                    if(buf_cap == 0)
                    {
                        buf = malloc(BUF_SIZE_UNIT);
                        if(!buf)
                        {
                            printf("Insufficient memory. Exiting\n");
                            //socket_file_descriptor needs to be freed.
                            //completion boolean has to be set
                            goto free_socket_fd;
                        }
                    }
                    else
                    {
                        int new_len = buf_cap + BUF_SIZE_UNIT; 
                        char *new_buf;   
                        new_buf = realloc(buf,new_len);     
                        if(!new_buf)
                        {
                            printf("Insufficient memory. Exiting\n");
                            free(buf);
                            //buf has to be freed
                            //socket_file_descriptor needs to be freed.
                            //completion boolean has to be set
                            goto free_mem;
                        }
                        buf = new_buf;           
                    }
                    buf_cap += BUF_SIZE_UNIT;
                }
                num_bytes_read = 0;
                num_bytes_read = recv(thread_params->socket_file_descriptor,(buf+buf_len),(buf_cap - buf_len),0);
                if(num_bytes_read == -1)
                {
                    syslog(LOG_ERR,"Recv: %s",strerror(errno));
                    //buf has to be freed
                    //socket_file_descriptor needs to be freed.
                    //completion boolean has to be set
                    goto free_mem;
                }
                else if(num_bytes_read>0)
                {
                    thread_params->curr_state = Parse_data;
                }
                else if(num_bytes_read == 0)
                {
                    //buf has to be freed
                    //socket_file_descriptor needs to be freed.
                    //completion boolean has to be set

                    goto free_mem;
                }
                break;
            case Parse_data:
                num_bytes_to_read = ((buf_len - start_ptr) + num_bytes_read);
                //printf("Bytes read %d num_bytes_to_read %d\n",num_bytes_read,num_bytes_to_read);
                int temp_read_var = num_bytes_to_read;
                for(ptr = &buf[start_ptr];temp_read_var>0;ptr++,temp_read_var--)
                {
                    if(*ptr == '\n')
                    {
                        temp_read_var--;
                        file_descriptor = open(LOG_FILE,O_RDWR|O_CREAT|O_APPEND,S_IRWXU|S_IRWXG|S_IRWXO);
                        if(file_descriptor == -1)
                        {
                            syslog(LOG_ERR,"Open: %s",strerror(errno));
                            goto free_mem;
                        }
                        int bytes_written_until_newline = (num_bytes_to_read - temp_read_var);
                        //printf("Temp var read %d\n",temp_read_var);
                        //if the socket sent AESDCHAR_IOCSEEKTO command then call the ioctl function
                        //otherwise write to file.
                        if(strncmp(&buf[start_ptr],commands[0].command,strlen(commands[0].command))==0)
                        {
                            struct aesd_seekto seekto;
                            //check if sscanf was successful
                            if(sscanf(&buf[start_ptr],"AESDCHAR_IOCSEEKTO:%u,%u",&seekto.write_cmd,&seekto.write_cmd_offset)!=2)
                            {
                                syslog(LOG_ERR,"Please enter valid arguments to AESDCHAR_IOCSEEKTO");
                            }
                            else
                            {
                                syslog(LOG_DEBUG,"Write cmd %d, offset %d\n",seekto.write_cmd,seekto.write_cmd_offset);
                                if(ioctl(file_descriptor,AESDCHAR_IOCSEEKTO,&seekto))
                                {
                                    syslog(LOG_ERR,"IOCTL: %s",strerror(errno));
                                }
                            }

                        }
                        else
                        {
                            //printf("Total buffsize %d buf len %d bytes written until newline %d \n",buf_cap,buf_len,bytes_written_until_newline);
                            if(dump_content(file_descriptor,&buf[start_ptr],bytes_written_until_newline)==-1)
                            {
                                //file_descriptor needs to be closed
                                //buf has to be freed
                                //socket_file_descriptor needs to be freed.
                                //completion boolean has to be set
                                //perform cleanup and exit, but this time unlock first
                                goto close_file_descriptor;
                            }                        
                        //printf("Total Bytes written to file %d\n",total_bytes_written_to_file);
                        }


                        if(echo_file_socket(file_descriptor,thread_params->socket_file_descriptor)==-1)
                        {
                            //file_descriptor needs to be closed
                            //buf has to be freed
                            //socket_file_descriptor needs to be freed.
                            //completion boolean has to be set
                            //perform cleanup and exit, but this time unlock first
                            goto close_file_descriptor;
                        }
                        start_ptr = bytes_written_until_newline;
                        close(file_descriptor);
                        break;
                    }
                }
                buf_len += num_bytes_read;
                thread_params->curr_state = Receive_From_Socket;
                break;
        }
    }
    close_file_descriptor: close(file_descriptor);
    //unlock_mutex: pthread_mutex_unlock(&socket_state.mutex);
    free_mem: free(buf);
    free_socket_fd: close(thread_params->socket_file_descriptor);
                    thread_params->thread_completed = true;
                    syslog(LOG_DEBUG,"Closed connection from %s",thread_params->ip_addr);
                    return 0;
}
#endif
/*
*   Writing the timestamp back to file is done from the main thread if there are no open connections,
*   otherwise it is done from the threads itself. This is to avoid skipping timestamps if the 
*   traffic is busy. 
*/
int main(int argc,char **argv)
{
    initialize_socket_state();
    bool run_as_daemon = false;
    main_thread_states_t main_thread_state;
    openlog(NULL,0,LOG_USER);
    int opt;
    while((opt = getopt(argc, argv,"d")) != -1)
    {
        switch(opt)
        {
            case 'd':
                run_as_daemon = true;
                break;
        }
    }
    int status=0,yes=1;
    struct addrinfo hints;
    struct addrinfo *p = NULL;  // will point to the results
    char s[INET6_ADDRSTRLEN];
    memset(s,0,sizeof(s));
    struct sockaddr_storage client_addr;
    socklen_t addr_size = sizeof(client_addr);
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
    //TODO: Call freeaddrinfo() once done with servinfo
    /*
    *   Get info regarding the peer at Port 9000
    */
    status = getaddrinfo(NULL, "9000", &hints, &socket_state.host_addr_info);
    if(status != 0)
    {
        syslog(LOG_ERR,"getaddrinfo: %s",strerror(errno));
        return -1;
    }
    socket_state.free_address_info = true;
    /*
    *   Try to bind to one of the socket descriptors returned by getaddrinfo
    */
    for(p = socket_state.host_addr_info; p != NULL; p = p->ai_next) 
    {
        socket_state.socket_descriptor = socket(p->ai_family, p->ai_socktype,p->ai_protocol);
        if(socket_state.socket_descriptor == -1)
        {
            syslog(LOG_ERR,"Socket: %s",strerror(errno));
            continue;
        }
        socket_state.free_socket_descriptor = true;
        status = setsockopt(socket_state.socket_descriptor,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
        if(status == -1)
        {
            syslog(LOG_ERR,"Set Socket Options: %s",strerror(errno)); 
            perform_cleanup();
            return -1;
        }
        status = bind(socket_state.socket_descriptor,p->ai_addr, p->ai_addrlen);
        if(status == -1)
        {
            syslog(LOG_ERR,"Bind: %s",strerror(errno));
            close(socket_state.socket_descriptor);
            continue;
        }
        break;
    }
    if(p == NULL)
    {
        syslog(LOG_ERR,"server: failed to bind");
        fprintf(stderr, "server: failed to bind\n");
        perform_cleanup();
        return -1;
    }
    
    
    freeaddrinfo(socket_state.host_addr_info);
    socket_state.free_address_info = false;
    /*
    *   if opt command line was specified, then run this program as a daemon
    */
    if(run_as_daemon)
    {
        pid_t pid;
        /* create new process */
        pid = fork ();
        if (pid == -1)
        {
            syslog(LOG_ERR,"Fork: %s",strerror(errno));
            perform_cleanup();
            return -1;
        }
        else if (pid != 0)
        {
            perform_cleanup();
            exit (EXIT_SUCCESS);
        }
        else
        {
            if(setsid()==-1)
            {
                syslog(LOG_ERR,"SetSid: %s",strerror(errno));
                perform_cleanup();
                return -1;
            }
            if(chdir("/")==-1)
            {
                syslog(LOG_ERR,"Chdir: %s",strerror(errno));;
                perform_cleanup();
                return -1;
            }
            /* redirect fd's 0,1,2 to /dev/null */
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
            open ("/dev/null", O_RDWR); /* stdin */
            dup (0); /* stdout */
            dup (0); /* stderror */
        }
    }
    int backlog = 10;
    printf("Listening for connections...\n");
    status = listen(socket_state.socket_descriptor,backlog);
    if(status == -1)
    {
        syslog(LOG_ERR,"Listen: %s",strerror(errno));
        perform_cleanup();
        return -1;
    }
    /*
    *   Setup signal handler
    */
    //TODO: Setup Signal Alarm 
    //Set up the signals handler
    
    if(setup_signal(SIGINT)== -1)
    {
        perform_cleanup();
        return -1;
    }
    if(setup_signal(SIGTERM)==-1)
    {
        perform_cleanup();
        return -1;        
    }
    #ifndef USE_AESD_CHAR_DEVICE
    initialize_circular_buf();
    if(setup_signal(SIGALRM)==-1)
    {
        perform_cleanup();
        return -1;          
    }
    if(create_and_arm_timer()==-1)
    {
        perform_cleanup();
        return -1;    
    }
    socket_state.disarm_alarm = true;
    #endif
    
    head_t head;
    TAILQ_INIT(&head);
    //Use inside thread
    /*
    *   Simple statemachine implemented to handle socket reading loop.
    *
    *   Accept_Connections -  accept new connections. Open socket file descriptor. If same IP as before, open /var/aesdsocketdata in append mode, else in truncate mode.
    *   Receive_From_Socket - Read data from socket. If no data received, close connection. Else send it for parsing.
    *   Parse_Data  -   Parse the data received character by character to check for '\n', if newline found, dump data until that character and echo back across socket. 
    */
    if(socket_state.signal_caught)
    {
        main_thread_state = Join_threads;
    }
    else
    {
        main_thread_state = Accept_Connections;
    }
    
    int socket_fd;
    while(1)
    {
        switch(main_thread_state)
        {
            case Accept_Connections:
                
                socket_fd = accept(socket_state.socket_descriptor,(struct sockaddr*)&client_addr,&addr_size);
                if(socket_fd == -1)
                {   
                    if(errno == EINTR)
                    {
                        printf("EINTR\n");
                        goto next_state;
                    }
                    syslog(LOG_ERR,"Accept: %s",strerror(errno));
                    goto next_state;
                }                    
                inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof(s));
                worker_thread_t *node = NULL;
                node = malloc(sizeof(worker_thread_t));
                if(!node)
                {
                    syslog(LOG_ERR,"Malloc failed");
                    goto next_state;
                }
                node->thread_completed = false;
                node->curr_state = Receive_From_Socket;
                node->socket_file_descriptor = socket_fd;
                strcpy(node->ip_addr,s);
                #ifdef USE_AESD_CHAR_DEVICE
                status = pthread_create(&node->thread_id,
                            (void*)0,
                            aesd_char_thread,
                            node);
                #else
                status = pthread_create(&node->thread_id,
                             (void*)0,
                             server_thread,
                             node);
                #endif
                if(status !=0)
                {
                    syslog(LOG_ERR,"Listen: %s",strerror(errno));
                    free(node);
                    goto next_state;
                }   
                TAILQ_INSERT_TAIL(&head, node, entries);
                socket_state.connection_count++;
                node = NULL;
                goto next_state;
                next_state:
                    main_thread_state = Join_threads;
                    break;
            case Join_threads:
                if(socket_state.connection_count>0)
                {
                    worker_thread_t *var = NULL;
                    worker_thread_t *tvar = NULL;
                    TAILQ_FOREACH_SAFE(var,&head,entries,tvar)
                    {
                        if(var->thread_completed)
                        {
                            pthread_join(var->thread_id,NULL);
                            TAILQ_REMOVE(&head, var, entries);
                            free(var);
                            var = NULL;
                            socket_state.connection_count--;
                        }
                    }
                }
                #ifndef USE_AESD_CHAR_DEVICE
                if(socket_state.connection_count==0)
                {
                    //trylock instead of lock to prevent race conditions ending up in a deadlock.
                    status = pthread_mutex_trylock(&socket_state.mutex);
                    if(status == 0)
                    {
                        char time_val_buf[80];
                        if(dequeue_data_from_circ_buf(time_val_buf)==0)
                        {
                            int fd = open(LOG_FILE,O_WRONLY|O_CREAT|O_APPEND,S_IRWXU|S_IRWXG|S_IRWXO);
                            if(fd != -1)
                            {
                                dump_content(fd,time_val_buf,strlen(time_val_buf));
                                close(fd);
                            }
                        }
                        pthread_mutex_unlock(&socket_state.mutex);
                    }
                }
                #endif
                if(socket_state.signal_caught)
                {
                    if(socket_state.connection_count==0)
                    {
                        shutdown_function();
                    }
                    else
                    {
                        break;
                    }
                }
                main_thread_state = Accept_Connections;
                break;
        }    
    }
}