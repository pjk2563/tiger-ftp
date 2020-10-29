#!/usr/bin/bash

# duplicate binary files in root directory for concurrency test

for i in {1..100}; do 
    cp 5mb.bin "server/5mb$i.bin"; 
    cp 3mb.bin "client/3mb$i.bin";
done

