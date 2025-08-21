#!/bin/bash

# this script is not needed any more, as the SCPI server can now parse multiple commands
# just do:
# nc 192.168.0.18 8888 < scpiexample.txt

# Check parameters
if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <file> <host> <port>"
    exit 1
fi

file="$1"
host="$2"
port="$3"

# Read file line by line and send each line
while IFS= read -r line; do
    echo "$line" | timeout 0.01 nc "$host" "$port"
done < "$file"

