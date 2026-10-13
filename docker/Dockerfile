FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential autoconf automake pkg-config libglib2.0-dev debhelper python3 \
    bridge-utils net-tools htop vim strace lsof \
    systemd systemd-sysv init-system-helpers logrotate \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN ./autogen.sh && make deb \
    && scripts/install-deb.sh ../softgred_*.deb \
    && scripts/check-installed-package.sh
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
    libglib2.0-0 ebtables iptables \
    bridge-utils net-tools htop vim strace lsof \
    systemd systemd-sysv init-system-helpers logrotate \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /softgred_*.deb /tmp/packages/
COPY scripts/install-deb.sh scripts/check-installed-package.sh /tmp/
RUN /tmp/install-deb.sh /tmp/packages/*.deb \
    && /tmp/check-installed-package.sh \
    && rm -rf /tmp/packages /tmp/install-deb.sh /tmp/check-installed-package.sh /var/lib/apt/lists/*
EXPOSE 8080
ENTRYPOINT ["python3", "/usr/share/softgred/run-services.py"]
CMD []
