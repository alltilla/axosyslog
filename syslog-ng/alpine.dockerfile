ARG DEBUG=false

FROM alpine:3.18 as apkbuilder

ARG PKG_TYPE=stable
ARG SNAPSHOT_VERSION
ARG DEBUG
ENV DEBUG=$DEBUG

RUN apk add --update-cache \
      alpine-conf \
      alpine-sdk \
      sudo \
    && apk upgrade -a \
    && adduser -D builder \
    && addgroup builder abuild

USER builder
WORKDIR /home/builder
ADD --chown=builder:builder apkbuild .

RUN [ $DEBUG = false ] || (cd axoflow/syslog-ng && patch -p1 -i APKBUILD-debug.patch)

RUN mkdir packages \
    && abuild-keygen -n -a \
    && printf 'export JOBS=$(nproc)\nexport MAKEFLAGS=-j$JOBS\n' >> .abuild/abuild.conf \
    && cd axoflow/syslog-ng \
    && if [ "$PKG_TYPE" = "nightly" ]; then \
        tarball_filename="$(ls *.tar.*)"; \
        [ -z "$tarball_filename" ] && echo "Tarball for nightly can not be found" && exit 1; \
        tarball_name="${tarball_filename/\.tar.*}"; \
        sed -i -e "s|^pkgver=.*|pkgver=$SNAPSHOT_VERSION|" -e "s|^builddir=.*|builddir=\"\$srcdir/$tarball_name\"|" APKBUILD; \
        sed -i -e "s|^source=.*|source=\"$tarball_filename\"|" APKBUILD; \
       fi \
    && abuild checksum \
    && abuild -r


FROM alpine:3.18

ARG DEBUG
ENV DEBUG=$DEBUG

# https://github.com/opencontainers/image-spec/blob/main/annotations.md
LABEL maintainer="axoflow.io"
LABEL org.opencontainers.image.title="AxoSyslog"
LABEL org.opencontainers.image.description="A cloud-native distribution of syslog-ng by Axoflow"
LABEL org.opencontainers.image.authors="Axoflow"
LABEL org.opencontainers.image.vendor="Axoflow"
LABEL org.opencontainers.image.licenses="GPL-3.0-only"
LABEL org.opencontainers.image.source="https://github.com/axoflow/axosyslog-docker"
LABEL org.opencontainers.image.documentation="https://axoflow.com/docs/axosyslog/docs/"
LABEL org.opencontainers.image.url="https://axoflow.io/"

COPY --from=apkbuilder /home/builder/packages/ /
COPY --from=apkbuilder /home/builder/.abuild/*.pub /etc/apk/keys/

RUN apk add --repository /axoflow -U --upgrade --no-cache \
    jemalloc \
    libdbi-drivers \
    syslog-ng \
    syslog-ng-add-contextual-data \
    syslog-ng-amqp \
    syslog-ng-cloud-auth \
    syslog-ng-ebpf \
    syslog-ng-examples \
    syslog-ng-geoip2 \
    syslog-ng-graphite \
    syslog-ng-grpc \
    syslog-ng-http \
    syslog-ng-json \
    syslog-ng-kafka \
    syslog-ng-map-value-pairs \
    syslog-ng-mongodb \
    syslog-ng-mqtt \
    syslog-ng-python3 \
    syslog-ng-redis \
    syslog-ng-riemann \
    syslog-ng-scl \
    syslog-ng-snmp \
    syslog-ng-sql \
    syslog-ng-stardate \
    syslog-ng-stomp \
    syslog-ng-tags-parser \
    syslog-ng-xml

RUN [ $DEBUG = false ] || apk add -U --upgrade --no-cache \
    gdb \
    valgrind \
    strace \
    perf \
    vim \
    musl-dbg

EXPOSE 514/udp
EXPOSE 601/tcp
EXPOSE 6514/tcp

HEALTHCHECK --interval=2m --timeout=5s --start-period=30s CMD /usr/sbin/syslog-ng-ctl healthcheck --timeout 5
ENV LD_PRELOAD /usr/lib/libjemalloc.so.2
ENTRYPOINT ["/usr/sbin/syslog-ng", "-F"]
