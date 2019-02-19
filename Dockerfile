FROM debian:stretch

RUN apt update && apt install build-essential pkg-config gcc \
        grunt libnl-3-dev libnl-genl-3-dev -y

COPY . /htop

WORKDIR /htop

RUN ./autogen.sh && ./configure --enable-delayacct && make

CMD [ "/htop/htop" ]
