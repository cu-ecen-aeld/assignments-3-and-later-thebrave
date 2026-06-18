#!/bin/bash

# Accepts the following runtime arguments: the first argument is a path to a directory on the filesystem, 
# referred to below as filesdir; the second argument is a text string which will be searched within these 
# files, referred to below as searchstr
# 
# Exits with return value 1 error and print statements if any of the parameters above were not specified
# 
# Exits with return value 1 error and print statements if filesdir does not represent a directory on the 
# filesystem
# 
# Prints a message "The number of files are X and the number of matching lines are Y" where X is the 
# number of files in the directory and all subdirectories and Y is the number of matching lines found in 
# respective files, where a matching line refers to a line which contains searchstr (and may also contain 
# additional content).

if [ "$#" -lt 2 ]; then
  echo "Not enough arguments provided"
  exit 1
elif [ ! -d "${1}" ]; then
  echo "$1 is not a directory"
  exit 1
fi

x="`find $1 -type f | wc --lines`"
y="`find $1 -type f | xargs grep -c "$2" | wc --lines`"

echo "The number of files are $x and the number of matching lines are $y"
