# Reverse Engineering ATC


### Perfmon

```bash
sudo perf stat -e dsa0/event=0x100, event_category=0x2/ -a ./build/gao
```
