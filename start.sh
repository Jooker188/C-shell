#!/bin/bash

FILE=$1
EXEC=$(echo $1 | awk -F "." '{print $1}')

gcc -g -Wall -Werror $FILE -o $EXEC

valgrind --leak-check=full ./$EXEC
