#!/usr/bin/bash

n=1
last=101
while [ $n -lt $last ]
do
     ./tigerC <<< "tconnect 127.0.0.1 user pass
     tget 5mb$n.bin
     tput 3mb$n.bin
     quit" &
     n=$(($n+1))
 done
