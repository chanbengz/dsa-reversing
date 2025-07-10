build:
	docker build --build-arg REPO="2506" -t vpp .

start:
	./startvpp1.sh
	docker run -it --name=vpp2 --privileged --network none --dns 114.114.114.114 \
    	-v /sys/bus/dsa/devices:/sys/bus/dsa/devices \
    	-v /dev/dsa:/dev/dsa \
    	-v /run/vpp/memif.sock:/run/vpp/memif.sock \
    	-v ./config:/vpp/config \
    	vpp /bin/bash

stop:
	ps -ef | grep vpp | awk '{print $$2}' | head -n 1 | xargs sudo kill
	sudo ip link del dev vpp1host
	# sudo iptables -t nat -D POSTROUTING -s 10.10.3.0/24 -j MASQUERADE
	# sudo iptables -D FORWARD -s 10.10.3.0/24 -i vpp1host -j ACCEPT          
    # sudo iptables -D FORWARD -s 10.10.3.0/24 -j ACCEPT

clean:
	docker rmi vpp

.PHONY: build start stop clean