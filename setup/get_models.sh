#!/bin/bash

models=(stories15M stories47M stories110M)
ollamas=(qwen3:1.7b gemma3:1b gemma3:4b qwen3:4b)

for m in "${models[@]}"; do
    wget https://huggingface.co/karpathy/tinyllamas/resolve/main/$m.bin
done

for m in "${ollamas[@]}"; do
    docker exec -it dto2 ollama pull $m
done