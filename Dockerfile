FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential autoconf automake pkg-config libglib2.0-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN ./autogen.sh --prefix=/usr --sysconfdir=/etc --localstatedir=/var \
    && make -j2 && make check && make install DESTDIR=/install
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends libglib2.0-0 ebtables iptables \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /install/ /
ENTRYPOINT ["/usr/sbin/softgred"]
CMD ["-f"]
