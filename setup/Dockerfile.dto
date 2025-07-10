FROM ubuntu:latest

WORKDIR /root

RUN apt update && apt install -y libaccel-config-dev \
    libaccel-config1 libnuma-dev ssh python3 \
    python3-paramiko unzip wget ca-certificates curl gnupg

COPY dto/libdto.so.1.0 /usr/lib64/libdto.so.1.0

RUN ln -sf /usr/lib64/libdto.so.1.0 /usr/lib64/libdto.so.1 && \
	ln -sf /usr/lib64/libdto.so.1.0 /usr/lib64/libdto.so

RUN wget https://storage.googleapis.com/chrome-for-testing-public/138.0.7204.92/linux64/chrome-headless-shell-linux64.zip && \
	unzip chrome-headless-shell-linux64.zip && cd chrome-headless-shell-linux64 && \
	while read pkg; do \
  		apt-get satisfy -y --no-install-recommends "${pkg}"; \
	done < ./deb.deps; apt-get clean && \
	rm -rf /var/lib/apt/lists/* /var/cache/apt/archives/* chrome-headless-shell-linux64.zip