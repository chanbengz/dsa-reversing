FROM ubuntu:latest

RUN apt-get update && apt-get install -y --no-install-recommends \
		apt-transport-https ca-certificates \
		curl gnupg iproute2 iputils-ping dnsutils \
 	&& rm -rf /var/lib/apt/lists/*

ARG REPO
ARG VPP_VERSION

WORKDIR /vpp

COPY get-vpp.sh /get-vpp.sh

RUN set -eux; \
	/get-vpp.sh; \
	apt-get update && apt-get install -y -V ./*.deb; \
	dpkg-query -f '${Version}\n' -W vpp > /vpp/version; \
	rm -rf vom*.deb vpp-dbg*.deb; \
	rm -rf /var/lib/apt/lists/*;

RUN mkdir -p /var/log/vpp && \
	curl -L -o lightpanda https://github.com/lightpanda-io/browser/releases/download/nightly/lightpanda-x86_64-linux && \
	chmod a+x ./lightpanda
