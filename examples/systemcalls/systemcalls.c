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
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
        printf("%s ",command[i]);
    }
    printf("\n");
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
    if (pid == -1)
    {
        perror("Fork");
        va_end(args);
        printf("FORK returning FALSE\n");
        return false;
    }
    else if (pid == 0) 
    {
        printf("command %s, vector1 %s vector2 %s\n",command[0],(command+1)[0],(command+1)[1]);
        if(execv (command[0], (command)) == -1)
        {
            perror("Exec");
            va_end(args);
            printf("EXEC returning FALSE\n");
            exit(-1);    
            //return false;
        }

    }
    else
    {
        if (waitpid (pid, &status, 0) == -1)
        {
            perror("Wait");
            va_end(args);
            printf("WAIT returning FALSE\n");
            return false;
        }
        else if (WIFEXITED (status))
        {
            int ret_status = WEXITSTATUS(status);
            printf("ret status 1 %d\n",ret_status);
            if(ret_status!=0)
            {
                printf("Command exited with non-zero status\n");
                va_end(args);
                printf("Command returning FALSE\n");
                //exit(-1);
                return false;
            }
            else
            {
                va_end(args);
                printf("ret status %d returning TRUE\n",ret_status);
                return true;
            }
        }
        va_end(args);
        printf("EXIT returning FALSE\n");
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
    printf("redirect\n");
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
        printf("%s ",command[i]);
    }
    printf("\nend of c\n");
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    //command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/
    int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if(fd<0)
    {
        perror("Open");
        return false;
    }

    
        pid_t pid;
    pid = fork ();
    if (pid == -1)
    {
        perror("Fork");
        va_end(args);
        printf("FORK returning FALSE\n");
        return false;
    }
    else if(pid == 0)
    {
        printf("In child\n");
        printf("\ncommand %s, vector1 %s vector2 %s\n",command[0],(command+1)[0],(command+1)[1]);
        if (dup2(fd, 1) < 0)
        { 
            perror("dup2"); 
            //printf("dup2 returning FALSE\n");
            return false; 
        }
        close(fd);
        //printf("\nEXECING\n");
        if(execv (command[0], (command))== -1)
        {            
            perror("Exec");
            va_end(args);
            printf("EXEC returning FALSE\n");
            exit(-1);
            //return false;
        }
    }
    else
    {
        close(fd);
        if (waitpid (pid, &status, 0) == -1)
        {
            perror("Wait");
            va_end(args);
            printf("Wait returning FALSE\n");
            return false;
        }
        else if (WIFEXITED (status))
        {
            int ret_status = WEXITSTATUS(status);
            if(ret_status!=0)
            {
                printf("Command exited with non-zero status %d\n",ret_status);
                va_end(args);
                printf("Command returning FALSE\n");
                return false;
            }
            else
            {
                va_end(args);
                printf(" returning True\n");
                return true;
            }
        }        
        va_end(args);
        printf("Exit returning FALSE\n");
        return false;
    }
    return false;
}
