#!/bin/bash

RUNS=200
WEB="web15"
mkdir -p ../evaluate/wfdata

if [ $# -eq 1 ]; then
    WEB=$1
fi

for i in $(seq 1 $RUNS); do
    echo "Running round $i/$RUNS"
    for web in $(cat $WEB.txt); do
        sudo taskset -c 8 ./build/monitor wf > /dev/null 2>&1 &
        sleep 0.8 && docker exec -it vpp2 /vpp/chrome-headless-shell \
            --headless=new --new-window --incognito --disable-application-cache \
            --disable-gpu --ignore-certificate-errors-spki-list \
            --enable-unsafe-swiftshader --disable-3d-apis --no-sandbox $web > /dev/null 2>&1 &
        sleep 5 && sudo pkill chrome-headless
        wait;
        mv wf-traces.txt wf-traces_$(echo "$web" | sed -E 's~https?://([^/]+).*~\1~')_$i.txt;
        sleep 1;
        echo "Finished $web"
    done
    mv wf*.txt ../evaluate/wfdata/
done