/*
*   file:      systemcalls.c
*   brief:     Executes a system call either as a system() argument or as a combination of fork, exec and wait function calls
*   author:    Guruprashanth Krishnakumar, gukr5411@colorado.edu, started code provided as part of assignment
*   date:      09/07/2022
*   refs:      Ch.5 of Linux System Programming by Robert Love, lecture slides of ECEN 5713 - Advanced Embedded Software Dev.
*/

/*
*   HEADER FILES
*/
#include "systemcalls.h"
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
    int ret = system(cmd);
    //if system call fails, return error.
    if(ret == -1)
    {
        perror("System error");
        return false;
    }
    if(ret > 0)
    {
        return false;
    }   
    
    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    //copy variable number of args passed to the command vector
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    //command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
    int status;
    pid_t pid;
    pid = fork ();
    //handle if fork failed
    if (pid == -1)
    {
        perror("Fork");
        va_end(args);
        return false;
    }
    //if child process
    else if (pid == 0) 
    {
        if(execv (command[0], (command)) == -1)
        {
            //if exec returns, that means it failed
            perror("Exec");
            va_end(args);
            exit(-1);    
        }

    }
    //if parent process
    else
    {
        if (waitpid (pid, &status, 0) == -1)
        {
            //if there's an issue with the invocation of the the waitpid function
            perror("Wait");
            va_end(args);
            return false;
        }
        //if the child process returned normally (did not exit by unnatural means like a signal)
        else if (WIFEXITED (status))
        {
            //check exit status of the child
            int ret_status = WEXITSTATUS(status);
            if(ret_status!=0)
            {
                printf("Command exited with non-zero status\n");
                va_end(args);
                return false;
            }
            //only if the child exited normally with a status of 0, return true
            else
            {
                va_end(args);
                return true;
            }
        }
        va_end(args);
        return false;
    }
    return false;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    int status;
    char * command[count+1];
    int i;
    //copy the variable number of args passed to the command vector
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/
    //open the file to redirect STDOUT to
    int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if(fd<0)
    {
        //if open failed
        perror("Open");
        return false;
    }

    
    pid_t pid;
    pid = fork ();
    if (pid == -1)
    {
        //if fork failed
        perror("Fork");
        va_end(args);
        return false;
    }
    //if child process
    else if(pid == 0)
    {
        //fd and 1 (which is STDOUT) both point to the same thing now i.e, STDOUT points to fd
        if (dup2(fd, 1) < 0)
        {
            //if dup failed. 
            perror("dup2"); 
            return false; 
        }
        close(fd);
        //exec the command
        if(execv (command[0], (command))== -1)
        {            
            //if exec returned
            perror("Exec");
            va_end(args);
            exit(-1);
        }
    }
    //if parent process
    else
    {
        close(fd);
        //wait for the child
        if (waitpid (pid, &status, 0) == -1)
        {
            perror("Wait");
            va_end(args);
            return false;
        }
        //if child exited normally
        else if (WIFEXITED (status))
        {
            int ret_status = WEXITSTATUS(status);
            //check exit status of child
            if(ret_status!=0)
            {
                printf("Command exited with non-zero status %d\n",ret_status);
                va_end(args);
                return false;
            }
            //if child returned normally and exit status = 0, then return true
            else
            {
                va_end(args);
                return true;
            }
        }        
        //if child did not return normally return false
        va_end(args);
        return false;
    }
    return false;
}
