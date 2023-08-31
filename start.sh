#!/bin/bash

FILE="$1"
EXEC=$(echo "$1" | awk -F "." '{print $1}')

gcc -Wall -Werror "$FILE" -o "$EXEC" && "./$EXEC"

