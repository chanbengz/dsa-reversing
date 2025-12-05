#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <atc|swq>"
    exit 1
fi

docker run -itd --rm --privileged --name=dtossh \
    -v /dev/dsa:/dev/dsa \
    -v /sys/bus/dsa:/sys/bus/dsa \
    --env-file dto.env \ 
    dto_dsa

docker run -itd --rm --name=ssh_server --hostname ssh_server ubuntu:latest
docker exec -d ssh_server "apt update"
docker exec -d ssh_server "apt install -y ssh"
docker exec -d ssh_server "service ssh start"
docker exec -d ssh_server "echo root:123456 | sudo chpasswd"
sleep 2

sudo taskset -c 8 ./build/($1)_spy ssh &
sleep 0.5;
docker exec -d dtossh "python sshev.py"
docker mv dtossh:/root/ground-ts.txt ../evaluate/data/$1-ground-ts.txt
mv $1-ssh-ts.txt ../evaluate/data/