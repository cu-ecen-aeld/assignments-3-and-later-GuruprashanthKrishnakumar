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

#define BUF_SIZE_UNIT           (1024)

bool clean_addr_info = true, clean_socket_descriptor = false, clean_socket_fd = false, clean_append_fd = false, free_append_string = false,run_as_daemon=false;
int socket_descriptor,socket_file_descriptor, append_file_descriptor;
// return -1 on failure
struct addrinfo *host_addr_info = NULL;
char *buf = NULL;
static void perform_cleanup();
static void sighandler();
static void *get_in_addr(struct sockaddr *sa);
static int dump_content(int fd, char* string,int write_len);
static int read_from_dump(int fd,int read_len);
static void perform_cleanup()
{
    if(host_addr_info && clean_addr_info)
    {
        freeaddrinfo(host_addr_info);
    }
    if(clean_socket_descriptor)
    {
        close(socket_descriptor);
    }
    if(clean_socket_fd)
    {
        close(socket_file_descriptor);
    }
    if(clean_append_fd)
    {
        close(append_file_descriptor);
    }
    if(free_append_string)
    {
        free(buf);
    }
}
//TODO: Delete file 
static void sighandler()
{
    printf("Caught Signal. Exiting\n");
    perform_cleanup();
    if(clean_append_fd)
    {
        printf("Deleting file\n");
        unlink("/var/tmp/aesdsocketdata");
    }
    exit(1);
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
            printf("Write len %d\n",write_len);
            perror("Error Write");
            return -1;
        }
        write_len -= ret;
        string += ret;
    }
    return 0;
}
static int read_from_dump(int fd,int read_len)
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
            printf("Read Len %d\n",read_len);
            perror("Read");
            return -1;
        }
        int num_bytes_to_send = ret;
        int num_bytes_sent = 0;
        int str_index = 0;
        while(num_bytes_to_send>0)
        {
            num_bytes_sent = send(socket_file_descriptor,&write_str[str_index],ret,0);
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
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    status = getaddrinfo(NULL, "9000", &hints, &host_addr_info);
    if(status != 0)
    {
        perror("GetAddrInfo");
        perform_cleanup();
        return -1;
    }
    for(p = host_addr_info; p != NULL; p = p->ai_next) 
    {
        socket_descriptor = socket(p->ai_family, p->ai_socktype,p->ai_protocol);
        if(socket_descriptor == -1)
        {
            perror("Socket");
            continue;
        }
        clean_socket_descriptor = true;
        status = setsockopt(socket_descriptor,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
        if(status == -1)
        {
            perror("Failed to set socket options");
            perform_cleanup();
            return -1;
        }
        status = bind(socket_descriptor,p->ai_addr, p->ai_addrlen);
        if(status == -1)
        {
            close(socket_descriptor);
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
    freeaddrinfo(host_addr_info);
    clean_addr_info = false;
    if(run_as_daemon)
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
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
        }
    }
    int backlog = 10;
    printf("Listening for connections...\n");
    status = listen(socket_descriptor,backlog);
    if(status == -1)
    {
        perror("Listen");
        perform_cleanup();
        return -1;
    }

    int total_bytes_written_to_file=0;
    //char *pptr;
    while(1)
    {
        bool new_line_found = false;
        socket_file_descriptor = accept(socket_descriptor,(struct sockaddr*)&client_addr,&addr_size);
        if(socket_file_descriptor == -1)
        {
            perror("Error Accepting Connection");
            perform_cleanup();
            return -1;
        }
        clean_socket_fd = true;
        inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof(s));
        printf("Accepted connection from %s\n", s);
        if(strcmp(prev_ip,s)==0)
        {
            printf("Matching IP. Opening in Append mode\n");
            append_file_descriptor = open("/var/tmp/aesdsocketdata",O_RDWR|O_CREAT|O_APPEND,S_IRWXU|S_IRWXG|S_IRWXO);
        }
        else
        {
            printf("New IP. Opening in Trunc mode\n");
            append_file_descriptor = open("/var/tmp/aesdsocketdata",O_RDWR|O_CREAT|O_TRUNC,S_IRWXU|S_IRWXG|S_IRWXO);   
        }
        strcpy(prev_ip,s);
        clean_append_fd = true;
        int num_bytes_read = 0;
        while(1)
        {
            if(buf_cap == buf_len)
            {
                if(buf_cap == 0)
                {
                    printf("malloc\n");
                    buf = malloc(BUF_SIZE_UNIT);
                    free_append_string = true;
                }
                else
                {
                    int new_len = buf_cap + BUF_SIZE_UNIT;
                    printf("Reall\n");    
                    buf = realloc(buf,new_len);                
                }
                if(!buf)
                {
                    printf("Insufficient memory. Exiting\n");
                    perform_cleanup();
                    return -1;
                }
                buf_cap += BUF_SIZE_UNIT;
            }
            num_bytes_read = recv(socket_file_descriptor,(buf+buf_len),(buf_cap - buf_len),0);
            if(num_bytes_read == -1)
            {
                perror("Recv");
                perform_cleanup();
                return -1;
            }
            else if(num_bytes_read > 0)
            {
                char *ptr;
                //number of bytes read in this recv
                int num_bytes_to_read = num_bytes_read;
                //start from buf_len and read all the bytes 
                for(ptr = &buf[buf_len];num_bytes_to_read>0;ptr++,num_bytes_to_read--)
                {
                    if(*ptr == '\n')
                    {
                        num_bytes_to_read--;
                        int bytes_written_until_newline = buf_len + (num_bytes_read - num_bytes_to_read);
                        printf("Total buffsize %d buf len %d bytes written until newline %d \n",buf_cap,buf_len,bytes_written_until_newline);
                        printf("Bytes to dump %d\n",bytes_written_until_newline);
                        if(dump_content(append_file_descriptor,buf,bytes_written_until_newline)==-1)
                        {
                            perform_cleanup();
                            return -1;
                        }
                        lseek( append_file_descriptor, 0, SEEK_SET );
                        total_bytes_written_to_file += bytes_written_until_newline;
                        if(read_from_dump(append_file_descriptor,total_bytes_written_to_file)==-1)
                        {
                            perform_cleanup();
                            return -1;
                        }
                        close(socket_file_descriptor);
                        clean_socket_fd = false;
                        buf_cap =0;
                        buf_len = 0;
                        free(buf);
                        free_append_string = false;
                        printf("Closed connection from %s\n",s);
                        new_line_found = true;
                        break;
                    }
                }
                if(!new_line_found)
                {
                    buf_len += BUF_SIZE_UNIT;
                }
                else
                {
                    break;
                }
            }
        }    
    }
}
