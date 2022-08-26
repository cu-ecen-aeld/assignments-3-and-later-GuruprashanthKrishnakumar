#!/bin/bash
if [ $# -lt 2 ]
then
    echo Please supply the directory name as arg1 and the string to search as arg2
    exit 1
fi
if [ -d $1 ]
then
    echo The number of files are $( ls $1 | wc -l ) and the number of matching lines are $( grep -R "$2" $1 | wc -l )
else
    echo directory specified was not present.
    exit 1
fi