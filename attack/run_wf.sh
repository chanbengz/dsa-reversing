#!/bin/bash

RUNS=200

for web in $(cat websites.txt); do
    echo "Running for $web"
    for i in $(seq 1 $RUNS); do
        sudo taskset -c 141 ./build/monitor > /dev/null 2>&1 &
        sleep 0.8 && docker exec -it vpp2 ./chrome-headless-shell --headless=new --new-window --incognito --disable-application-cache --disable-gpu  --no-sandbox $web > /dev/null 2>&1 &
        sleep 5 && pkill chrome-headless-shell
        wait;
        mv wf-traces.txt wf-traces_$(echo "$web" | sed -E 's~https?://([^/]+).*~\1~')_$i.txt;
        sleep 1;
    done

    mv wf*.txt ../evaluate/wfdata/
done