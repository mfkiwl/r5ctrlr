#!/bin/bash

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

