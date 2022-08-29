#!/bin/bash
#Commands leveraged from lecture slides as well as internet (stack overflow)
#if number of arguments are less than 2 return error
if [ $# -lt 2 ]
then
    echo Please supply the directory name as arg1 and the string to search as arg2
    exit 1
fi
#if subdirectories in the argument passed are not present, create the subdirectories first.
mkdir -p "$(dirname "$1")" 
#check if the previous command was successful
if [ $? -eq 1 ] 
then
    echo File could not be created
    exit 1
else
    echo File created and string written
fi
#write string to text file.
echo $2 > $1