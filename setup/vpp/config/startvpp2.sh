#!/bin/bash

# Environment variables for the vpp-container-fun multiple setup
source env.sh

vpp -c startup2.conf

# Setup the host link
ip link add name vpp2out type veth peer name vpp2host
ip link set dev vpp2out up
ip link set dev vpp2host up
ip addr add "${HOST2IP}"/"${HOST2MASK}" dev vpp2host
ip route add "${HOSTROUTE}"/"${HOSTMASK}" via "${VPP2HOSTINTIP}"

# Make sure VPP is *really* running
typeset -i cnt=60
until ls -l /run/vpp/cli-vpp2.sock ; do
       ((cnt=cnt-1)) || exit 1
       sleep 1
done

vppctl -s /run/vpp/cli-vpp2.sock create host-interface name vpp2out
typeset -i cnt=60
until vppctl -s /run/vpp/cli-vpp2.sock show int | grep vpp2out ; do
       ((cnt=cnt-1)) || exit 1
       sleep 1
       vppctl -s /run/vpp/cli-vpp2.sock create host-interface name vpp2out
done

vppctl -s /run/vpp/cli-vpp2.sock create host-interface name vpp2out
vppctl -s /run/vpp/cli-vpp2.sock set int state host-vpp2out up
vppctl -s /run/vpp/cli-vpp2.sock set int ip address host-vpp2outcreate host-interface name vpp2out "${VPP2HOSTINTIP}"/"${VPP2HOSTINTMASK}"