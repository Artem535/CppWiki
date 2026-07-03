# syntax=docker/dockerfile:1.7

ARG UBUNTU_VERSION=24.04

FROM ubuntu:${UBUNTU_VERSION} AS builder

ARG DEBIAN_FRONTEND=noninteractive
ARG VCPKG_COMMIT=195276f71622bc41392db5c5c5c5141c95ff9a36

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        autoconf \
        automake \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        git \
        libbz2-dev \
        libicu-dev \
        libjemalloc-dev \
        liblzma-dev \
        libssl-dev \
        libtool \
        libzstd-dev \
        ninja-build \
        perl \
        pkg-config \
        python3 \
        python3-venv \
        tar \
        unzip \
        zip \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/microsoft/vcpkg.git /opt/vcpkg \
    && git -C /opt/vcpkg checkout "${VCPKG_COMMIT}" \
    && /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics

ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"

WORKDIR /src
COPY vcpkg.json ./

RUN --mount=type=cache,target=/root/.cache/vcpkg \
    vcpkg install \
        --triplet x64-linux \
        --x-install-root=/src/build/server-release/vcpkg_installed

COPY . .

RUN --mount=type=cache,target=/root/.cache/vcpkg \
    cmake --preset server-release

RUN cmake --build --preset server-release --parallel

FROM ubuntu:${UBUNTU_VERSION} AS runtime

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        libatomic1 \
        libbz2-1.0 \
        libgcc-s1 \
        libicu74 \
        libjemalloc2 \
        liblzma5 \
        libstdc++6 \
        libzstd1 \
        zlib1g \
    && rm -rf /var/lib/apt/lists/*

RUN useradd --system --create-home --home-dir /var/lib/cppwiki --shell /usr/sbin/nologin cppwiki \
    && mkdir -p /etc/cppwiki \
    && chown -R cppwiki:cppwiki /etc/cppwiki /var/lib/cppwiki

COPY --from=builder /src/build/server-release/src/cppwiki-server /usr/local/bin/cppwiki-server
COPY config/server.docker.yaml /etc/cppwiki/server.yaml

USER cppwiki
EXPOSE 8080

ENTRYPOINT ["/usr/local/bin/cppwiki-server"]
CMD ["-c", "/etc/cppwiki/server.yaml"]
