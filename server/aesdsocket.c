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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/*
*   MACROS
*/
#define BUF_SIZE_UNIT           (1024)

/*
*   GLOBALS
*/
typedef enum
{
    Accept_Connections,
    Receive_From_Socket,
    Parse_data,
}states_t;

typedef struct
{
    bool clean_addr_info;
    bool clean_socket_descriptor;
    bool clean_socket_fd;
    bool clean_append_fd;
    bool free_append_string;
    bool run_as_daemon;
    bool reading_data;
    bool signal_caught;
    int socket_descriptor;
    int socket_file_descriptor;
    int append_file_descriptor;
    struct addrinfo *host_addr_info;
    char *buf;
    states_t curr_state;
}socket_state_struct_t;

socket_state_struct_t socket_state;

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
static int echo_file_socket(int fd,int read_len);

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
static void initialize_socket_state()
{

    socket_state.clean_addr_info = true;
    socket_state.clean_socket_descriptor = false;
    socket_state.clean_socket_fd = false;
    socket_state.clean_append_fd = false;
    socket_state.free_append_string = false;
    socket_state.run_as_daemon = false;
    socket_state.reading_data = false;
    socket_state.signal_caught = false;
    socket_state.host_addr_info = NULL;
    socket_state.buf = NULL;
}
static void perform_cleanup()
{
    if(socket_state.host_addr_info && socket_state.clean_addr_info)
    {
        freeaddrinfo(socket_state.host_addr_info);
    }
    if(socket_state.clean_socket_descriptor)
    {
        close(socket_state.socket_descriptor);
    }
    if(socket_state.clean_socket_fd)
    {
        close(socket_state.socket_file_descriptor);
    }
    if(socket_state.clean_append_fd)
    {
        close(socket_state.append_file_descriptor);
    }
    if(socket_state.free_append_string)
    {
        free(socket_state.buf);
    }
}
static void shutdown_function()
{
    printf("\nCaught Signal. Exiting\n");
    perform_cleanup();
    if(socket_state.clean_append_fd)
    {
        printf("Deleting file\n");
        unlink("/var/tmp/aesdsocketdata");
    }
    exit(1);
}

static void sighandler()
{
    
    if(!socket_state.reading_data)
    {
        shutdown_function();
    }
    else
    {
        socket_state.signal_caught = true;
    }

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
            perror("Error Write");
            return -1;
        }
        write_len -= ret;
        string += ret;
    }
    return 0;
}
static int echo_file_socket(int fd,int read_len)
{
    ssize_t ret; 
    char write_str[BUF_SIZE_UNIT];
    while(read_len!=0)
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
            perror("Read");
            return -1;
        }
        int num_bytes_to_send = ret;
        int num_bytes_sent = 0;
        int str_index = 0;
        while(num_bytes_to_send>0)
        {
            num_bytes_sent = send(socket_state.socket_file_descriptor,&write_str[str_index],num_bytes_to_send,0);
            if(num_bytes_sent == -1)
            {
                perror("Send");
                return -1;
            }
            num_bytes_to_send -= num_bytes_sent;
            str_index += num_bytes_sent;
        }
        read_len -= ret;
    }
    return 0;
}

int main(int argc,char **argv)
{
    initialize_socket_state();
    int opt;
    while((opt = getopt(argc, argv,"d")) != -1)
    {
        switch(opt)
        {
            case 'd':
                socket_state.run_as_daemon = true;
                break;
        }

    }
    int status=0,yes=1,buf_len=0,buf_cap=0;
    struct addrinfo hints;
    struct addrinfo *p = NULL;  // will point to the results
    char s[INET6_ADDRSTRLEN],prev_ip[INET6_ADDRSTRLEN];
    memset(s,0,sizeof(s));
    memset(prev_ip,0,sizeof(s));
    struct sockaddr_storage client_addr;
    socklen_t addr_size = sizeof(client_addr);
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
    //TODO: Call freeaddrinfo() once done with servinfo

    /*
    *   Setup signal handler
    */
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    /*
    *   Get info regarding the peer at Port 9000
    */
    status = getaddrinfo(NULL, "9000", &hints, &socket_state.host_addr_info);
    if(status != 0)
    {
        perror("GetAddrInfo");
        perform_cleanup();
        return -1;
    }

    /*
    *   Try to bind to one of the socket descriptors returned by getaddrinfo
    */
    for(p = socket_state.host_addr_info; p != NULL; p = p->ai_next) 
    {
        socket_state.socket_descriptor = socket(p->ai_family, p->ai_socktype,p->ai_protocol);
        if(socket_state.socket_descriptor == -1)
        {
            perror("Socket");
            continue;
        }
        socket_state.clean_socket_descriptor = true;
        status = setsockopt(socket_state.socket_descriptor,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
        if(status == -1)
        {
            perror("Failed to set socket options");
            perform_cleanup();
            return -1;
        }
        status = bind(socket_state.socket_descriptor,p->ai_addr, p->ai_addrlen);
        if(status == -1)
        {
            close(socket_state.socket_descriptor);
            perror("server: bind");
            continue;
        }
        break;
    }
    if(p == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        perform_cleanup();
        return -1;
    }
    
    freeaddrinfo(socket_state.host_addr_info);
    socket_state.clean_addr_info = false;
    /*
    *   if opt command line was specified, then run this program as a daemon
    */
    if(socket_state.run_as_daemon)
    {
        pid_t pid;
        /* create new process */
        pid = fork ();
        if (pid == -1)
        {
            perror("Fork");
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
                perror("Session");
                perform_cleanup();
                return -1;
            }
            if(chdir("/")==-1)
            {
                perror("Changing directory");
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
        perror("Listen");
        perform_cleanup();
        return -1;
    }
    
    int total_bytes_written_to_file=0;
    socket_state.curr_state = Accept_Connections;
    int num_bytes_read = 0;
    int start_ptr = 0;
    int num_bytes_to_read;
    char *ptr;
    /*
    *   Simple statemachine implemented to handle socket reading loop.
    *
    *   Accept_Connections -  accept new connections. Open socket file descriptor. If same IP as before, open /var/aesdsocketdata in append mode, else in truncate mode.
    *   Receive_From_Socket - Read data from socket. If no data received, close connection. Else send it for parsing.
    *   Parse_Data  -   Parse the data received character by character to check for '\n', if newline found, dump data until that character and echo back across socket. 
    */
    while(1)
    {
        
        switch(socket_state.curr_state)
        {
            case Accept_Connections:
                socket_state.socket_file_descriptor = accept(socket_state.socket_descriptor,(struct sockaddr*)&client_addr,&addr_size);
                if(socket_state.socket_file_descriptor == -1)
                {
                    perror("Error Accepting Connection");
                    perform_cleanup();
                    return -1;
                }                    
                socket_state.clean_socket_fd = true;
                inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof(s));
                printf("Accepted connection from %s\n", s);
                //socket_state.append_file_descriptor = open("/var/tmp/aesdsocketdata",O_RDWR|O_CREAT|O_APPEND,S_IRWXU|S_IRWXG|S_IRWXO);
                if(strcmp(prev_ip,s)==0)
                {
                    //printf("Matching IP. Opening in Append mode\n");
                    socket_state.append_file_descriptor = open("/var/tmp/aesdsocketdata",O_RDWR|O_CREAT|O_APPEND,S_IRWXU|S_IRWXG|S_IRWXO);
                }
                else
                {
                    printf("New IP. Opening in Trunc mode\n");
                    socket_state.append_file_descriptor = open("/var/tmp/aesdsocketdata",O_RDWR|O_CREAT|O_TRUNC,S_IRWXU|S_IRWXG|S_IRWXO);   
                }
                strcpy(prev_ip,s);
                socket_state.clean_append_fd = true;
                socket_state.reading_data = true;
                socket_state.curr_state = Receive_From_Socket;
                break;
            case Receive_From_Socket:
                if(buf_cap == buf_len)
                {
                    if(buf_cap == 0)
                    {
                        socket_state.buf = malloc(BUF_SIZE_UNIT);
                        socket_state.free_append_string = true;
                    }
                    else
                    {
                        int new_len = buf_cap + BUF_SIZE_UNIT;    
                        socket_state.buf = realloc(socket_state.buf,new_len);                
                    }
                    if(!socket_state.buf)
                    {
                        printf("Insufficient memory. Exiting\n");
                        perform_cleanup();
                        return -1;
                    }
                    buf_cap += BUF_SIZE_UNIT;
                }
                num_bytes_read = 0;
                num_bytes_read = recv(socket_state.socket_file_descriptor,(socket_state.buf+buf_len),(buf_cap - buf_len),0);
                if(num_bytes_read == -1)
                {
                    perror("Recv");
                    perform_cleanup();
                    return -1;
                }
                else if(num_bytes_read>0)
                {
                    socket_state.curr_state = Parse_data;
                }
                else if(num_bytes_read == 0)
                {
                    //TODO, perform cleanup
                    close(socket_state.socket_file_descriptor);
                    socket_state.clean_socket_fd = false;
                    buf_cap =0;
                    buf_len = 0;
                    start_ptr = 0;
                    free(socket_state.buf);
                    socket_state.free_append_string = false;
                    socket_state.reading_data = false;
                    if(socket_state.signal_caught)
                    {
                        shutdown_function();
                    }
                    printf("Closed connection from %s\n", s);
                    socket_state.curr_state = Accept_Connections;
                }
                break;
            case Parse_data:
                num_bytes_to_read = ((buf_len - start_ptr) + num_bytes_read);
                //printf("Bytes read %d num_bytes_to_read %d\n",num_bytes_read,num_bytes_to_read);
                int temp_read_var = num_bytes_to_read;
                for(ptr = &socket_state.buf[start_ptr];temp_read_var>0;ptr++,temp_read_var--)
                {
                    if(*ptr == '\n')
                    {
                        temp_read_var--;
                        //printf("Temp var read %d\n",temp_read_var);
                        int bytes_written_until_newline = (num_bytes_to_read - temp_read_var);
                        //printf("Total buffsize %d buf len %d bytes written until newline %d \n",buf_cap,buf_len,bytes_written_until_newline);
                        if(dump_content(socket_state.append_file_descriptor,&socket_state.buf[start_ptr],bytes_written_until_newline)==-1)
                        {
                            perform_cleanup();
                            return -1;
                        }                        
                        lseek(socket_state.append_file_descriptor, 0, SEEK_SET );
                        total_bytes_written_to_file += bytes_written_until_newline;
                        //printf("Total Bytes written to file %d\n",total_bytes_written_to_file);

                        if(echo_file_socket(socket_state.append_file_descriptor,total_bytes_written_to_file)==-1)
                        {
                            perform_cleanup();
                            return -1;
                        }
                        start_ptr = bytes_written_until_newline;
                        break;
                    }
                }
                buf_len += num_bytes_read;
                socket_state.curr_state = Receive_From_Socket;
                break;
        }    
    }
}
