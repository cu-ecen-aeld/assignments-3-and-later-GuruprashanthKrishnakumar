/*
*   file:      writer.c
*   brief:     implements the functionality of writer.sh. Takes 2 arguments <text-file-path> and <string> and writes the string into the file.
*              Assumes that the path to the file already exists.
*   author:    Guruprashanth Krishnakumar, gukr5411@colorado.edu
*   date:      09/04/2022
*   refs:      Ch.2 of Linux System Programming by Robert Love, lecture slides of ECEN 5713 - Advanced Embedded Software Dev.
*/

/*
*   HEADER FILES
*/
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

/*
*   FUNCTION DEFINITIONs
*/

/*
*  Logs a usage message in case the user invoked the function with the wrong number of arguments
*  args:
*       none
*  return:
        none
*/
static void usage()
{
    syslog(LOG_ERR,"Usage: ./writer <absolute filepath including filename> <string to write>");
}

/*
*  Opens the file present in the argument passed
*  args:
*       file_path - the path of the file that needs to be opened
*  return:
        file descriptor of the opened file
*/
static int open_file(char* file_path)
{
    //Execute permissions will not be given for OTHERs because umask = 0002
    int fd = open(file_path,O_WRONLY|O_CREAT|O_TRUNC,S_IRWXU|S_IRWXG|S_IRWXO);
    if(fd == -1)
    {
        syslog(LOG_ERR,"Error: %s",strerror(errno));
        exit(1);
    }
    return fd;
}

/*
*  Writes string to a file
*  args:
*       fd - file descriptor of the file
*       string - string that needs to be written
*  return:
        none
*/
static void write_string(int fd, char *string)
{
    int write_len = strlen(string);
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
            syslog(LOG_ERR,"Error: %s",strerror(errno));
            exit(1);
        }
        write_len -= ret;
        string += ret;
    }
}

/*
*  Closes a file
*  args:
*       fd - file descriptor of the file
*  return:
        none
*/
static void close_file(int fd)
{
    if(close(fd) == -1)
    {
        syslog(LOG_ERR,"Error: %s",strerror(errno));
    }
}

/*
*  Application entry point. Calls the appropriate functions needed for the application to work
*  args:
*       command line args
*  return:
        none
*/
int main( int argc, char *argv[] )
{
    openlog(NULL,0,LOG_USER);
    if(argc<3)
    {
        usage();
        exit(1);
    }
    int fd = open_file(argv[1]);
    syslog(LOG_DEBUG,"DEBUG: Writing %s to %s",argv[2],argv[1]);
    write_string(fd,argv[2]);
    close_file(fd);
}