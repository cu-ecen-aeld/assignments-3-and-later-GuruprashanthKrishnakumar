#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
static void usage()
{
    syslog(LOG_ERR,"Usage: writer <absolute filepath including filename> <string to write>");
}
static int open_file(char* file_path)
{
    //printf("File path %s\n",file_path);
    int fd = open(file_path,O_RDWR|O_CREAT|O_TRUNC,S_IRWXU|S_IRWXG|S_IRWXO);
    if(fd == -1)
    {
        syslog(LOG_ERR,"Error: %s",strerror(errno));
        //printf("Error opening/creating file specified\n");
        //syslog(LOG_ERR,"Error opening/creating file specified");
        exit(1);
    }
    return fd;
}
static void write_string(int fd, char *string)
{
    int write_len = strlen(string);
    //printf("Size of string %d\n",write_len);
    ssize_t ret;
    
    while(write_len!=0)
    {
        ret = write(fd,string,write_len);
        if(ret == 0)
        {
            break;
        } 
        //printf("ret = %ld\n",ret);
        if(ret == -1)
        {
            if(errno == EINTR)
            {
                continue;
            }
            syslog(LOG_ERR,"Error: %s",strerror(errno));
            exit(1);
        }
        write_len -= ret;
        string += ret;
    }
}
static void close_file(int fd)
{
    if(close(fd) == -1)
    {
        syslog(LOG_ERR,"Error: %s",strerror(errno));
    }
}
int main( int argc, char *argv[] )
{
    openlog(NULL,0,LOG_USER);
    //printf("Argc %d\n",argc);
    if(argc<3)
    {
        usage();
        exit(1);
    }
    //printf("length of string is %ld\n",strlen(argv[2]));
    int fd = open_file(argv[1]);
    syslog(LOG_DEBUG,"DEBUG: Writing %s to %s",argv[2],argv[1]);
    write_string(fd,argv[2]);
    //printf("Arg1 %s Arg2 %s \n",argv[1],argv[2]);
    close_file(fd);
}