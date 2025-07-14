#!/bin/bash

# Environment variables for the setup
export VPP1MEMIFIP=10.10.2.1
export VPP1MEMIFMASK=24
export VPP1HOSTINTIP=10.10.1.2
export VPP1HOSTINTMASK=24
export VPP2MEMIFIP=10.10.2.2
export VPP2MEMIFMASK=24
export VPP2HOSTINTIP=10.10.3.2
export VPP2HOSTINTMASK=24
export MEMIFROUTE=10.10.2.0
export MEMIFMASK=24
export VPP2ROUTE=10.10.3.0
export VPP2MASK=24
export HOST1IP=10.10.1.1
export HOST1MASK=24
export HOST2IP=10.10.3.1
export HOST2MASK=24
export HOSTROUTE=0.0.0.0
export HOSTMASK=0

# Run the VPP daemon
sudo vpp -c config/startup1.conf

# Make sure VPP is *really* running
typeset -i cnt=60
until ls -l /run/vpp/cli-vpp1.sock ; do
       ((cnt=cnt-1)) || exit 1
       sleep 1
done

# Setup the host link
sudo ip link add name vpp1out type veth peer name vpp1host
sudo ip link set dev vpp1out up
sudo ip link set dev vpp1host up
sudo ip addr add "${HOST1IP}"/"${HOST1MASK}" dev vpp1host
sudo ip route add "${MEMIFROUTE}"/"${MEMIFMASK}" via "${VPP1HOSTINTIP}"
sudo ip route add "${VPP2ROUTE}"/"${VPP2MASK}" via "${VPP1HOSTINTIP}"
# sudo iptables -t nat -A POSTROUTING -s 10.10.3.0/24 -j MASQUERADE
# sudo iptables -A FORWARD -s 10.10.3.0/24 -j ACCEPT
# sudo iptables -A FORWARD -d 10.10.3.0/24 -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT 

sudo vppctl -s /run/vpp/cli-vpp1.sock create interface af_xdp host-if vpp1out num-rx-queues all
typeset -i cnt=60
until sudo vppctl -s /run/vpp/cli-vpp1.sock show int | grep vpp1out ; do
       ((cnt=cnt-1)) || exit 1
       sleep 1
       sudo vppctl -s /run/vpp/cli-vpp1.sock create interface af_xdp host-if vpp1out num-rx-queues all
done

sudo vppctl -s /run/vpp/cli-vpp1.sock create interface memif id 0 master use-dma
sudo vppctl -s /run/vpp/cli-vpp1.sock set int state memif0/0 up
sudo vppctl -s /run/vpp/cli-vpp1.sock set int ip address memif0/0 ${VPP1MEMIFIP}/${VPP1MEMIFMASK}
sudo vppctl -s /run/vpp/cli-vpp1.sock ip route add ${VPP2ROUTE}/${VPP2MASK} via ${VPP2MEMIFIP}
sudo vppctl -s /run/vpp/cli-vpp1.sock ip route add ${HOSTROUTE}/${HOSTMASK} via ${HOST1IP}
sudo vppctl -s /run/vpp/cli-vpp1.sock set int state vpp1out/0 up
sudo vppctl -s /run/vpp/cli-vpp1.sock set int ip address vpp1out/0 "${VPP1HOSTINTIP}"/"${VPP1HOSTINTMASK}"
sudo vppctl -s /run/vpp/cli-vpp1.sock set int rx-mode vpp1out/0 queue 0 interrupt