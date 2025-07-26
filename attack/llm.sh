#!/bin/bash

RUNS=50
mkdir -p ../evaluate/mldata
models=(stories15M stories47M stories110M llama2-7b)
ollamas=(qwen3:1.7b gemma3:1b) # gemma3:4b qwen3:4b)

for m in "${ollamas[@]}"; do
    echo "Running $m"
    for i in $(seq 0 $RUNS); do
        sudo taskset -c 70 ./build/monitor ml > /dev/null 2>&1 &
        sleep 1 && docker exec -it dto2 ollama run --think=false $m "Once upon a time" > /dev/null 2>&1 &
        wait;
        mv ml-traces.txt ml-traces_$(echo $m)_$i.txt;
        sleep 2;
    done
    mv ml*.txt ../evaluate/mldata/
done

for i in $(seq 1 $RUNS); do
    echo "Running round $i/$RUNS"
    for m in "${models[@]}"; do
        sudo taskset -c 70 ./build/monitor ml > /dev/null 2>&1 &
        sleep 1 && docker exec -it dto1 ./run $m.bin -i "Once upon a time" > /dev/null 2>&1 &
        sleep 25 && sudo pkill run
        wait;
        mv ml-traces.txt ml-traces_$(echo $m)_$i.txt;
        echo "Finished $m"
        sleep 2;
    done
    mv ml*.txt ../evaluate/mldata/
done