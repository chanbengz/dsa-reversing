# Reverse Engineering ATC

## Structure
```
atc
├── dsa.h
├── gao.c
├── Makefile
├── README.md
```

### Perfmon

> [!WARNING]
> This method is deprecated because we observe
> that it will report a cache hit even if the 
> lower 32 bits are same while the upper 32 bits
> are different. Please find detail in .

```bash
sudo perf stat -e dsa0/event=0x100, event_category=0x2/ -a ./build/gao
```
