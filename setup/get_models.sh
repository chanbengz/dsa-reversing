#!/bin/bash

models=(stories15M stories47M stories110M)
ollamas=(qwen3:1.7b gemma3:1b gemma3:4b qwen3:4b)

if [ ! -z $(docker ps -q -f name=dto2) ]; then
    docker run -itd --rm --name=dto1 --privileged \
            -v /dev/dsa:/dev/dsa \
            -v /sys/bus/dsa:/sys/bus/dsa \
            -v ./llama2.c:/root/llama \
            --env-file ~/dsa-reversing/setup/dto.env \
            dto

    docker run -itd --rm --gpus '"device=0"' --name=dto2 --privileged \
           -v /dev/dsa:/dev/dsa \
           -v /sys/bus/dsa:/sys/bus/dsa \
           -v ./models:/root/models \
           --env-file ~/dsa-reversing/setup/dto.env dto-llm
fi

for m in "${models[@]}"; do
    wget https://huggingface.co/karpathy/tinyllamas/resolve/main/$m.bin
done

for m in "${ollamas[@]}"; do
    docker exec -it dto2 ollama pull $m
done