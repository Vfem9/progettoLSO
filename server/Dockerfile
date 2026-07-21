FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    gcc \
    make \
    libc6-dev \
    netcat-openbsd \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY include ./include
COPY src ./src
COPY Makefile ./

RUN make clean && make

EXPOSE 5000

CMD ["./bin/forza4_server"]
