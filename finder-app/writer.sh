#!/bin/bash
if [ $# -lt 2 ]
then
    echo Please supply the directory name as arg1 and the string to search as arg2
    exit 1
fi
mkdir -p "$(dirname "$1")" 
if [ $? -eq 1 ] 
then
    echo File could not be created
    exit 1
else
    echo File created and string written
fi
echo $2 > $1