#!/bin/bash
if [ $1 == "atc" ]; then
    echo "Running ATC covert channel..."
    for i in {1..40}; do
        sudo taskset -c 190 ./build/cc_receiver &
        sleep 0.2 && sudo taskset -c 188 ./build/cc_sender;
        wait;
        mv recv recv_$i;
        echo "Run $i completed";
        sleep 0.1;
    done
else
    echo "Running SWQ covert channel..."
    for i in {1..40}; do
        sudo taskset -c 190 ./build/cc_rworkq &
        sleep 0.2 && sudo taskset -c 188 ./build/cc_sworkq;
        wait;
        mv recv recv_$i;
        echo "Run $i completed";
        sleep 0.1;
    done
fi

mv recv* ../evaluate/data