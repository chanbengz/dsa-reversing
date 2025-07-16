FROM ubuntu:latest

ARG REPO
ARG VPP_VERSION

WORKDIR /vpp

RUN apt-get update && apt-get install -y --no-install-recommends \
		apt-transport-https ca-certificates unzip \
		curl wget gnupg iproute2 iputils-ping dnsutils

RUN wget https://storage.googleapis.com/chrome-for-testing-public/138.0.7204.157/linux64/chrome-headless-shell-linux64.zip && \
	unzip chrome-headless-shell-linux64.zip && cd chrome-headless-shell-linux64 && \
	while read pkg; do \
  		apt-get satisfy -y --no-install-recommends "${pkg}"; \
	done < ./deb.deps; apt-get clean && \
	rm -rf /var/lib/apt/lists/* /var/cache/apt/archives/*; \
	cd ../ && rm chrome-headless-shell-linux64.zip; \
	mv chrome-headless-shell-linux64/* ./;

COPY get-vpp.sh /get-vpp.sh

RUN set -eux; \
	/get-vpp.sh; \
	apt-get update && apt-get install -y -V ./*.deb; \
	dpkg-query -f '${Version}\n' -W vpp > /vpp/version; \
	rm -rf *.deb; \
	rm -rf /var/lib/apt/lists/* /var/cache/apt/archives/*;