#!/bin/bash
#Commands leveraged from lecture slides as well as internet (stack overflow)
#if number of arguments are less than 2 return error
if [ $# -lt 2 ]
then
    echo Please supply the directory name as arg1 and the string to search as arg2
    exit 1
fi
#check if argument passed is a directory
if [ -d $1 ]
#return number of files in the directory as well as all the subdirectories and the number of matching lines in those files
then
    echo The number of files are $( find $1 -type f| wc -l ) and the number of matching lines are $( grep -R "$2" $1 | wc -l )
#return error if the passed argument is not a directory.
else
    echo directory specified was not present.
    exit 1
fi
