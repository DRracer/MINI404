FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# QEMU 9.2.1 build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    gcc \
    g++ \
    meson \
    ninja-build \
    python3 \
    python3-venv \
    python3-pip \
    python3-setuptools \
    pkg-config \
    # glib (the reason we need this container)
    libglib2.0-dev \
    # Display — GTK3
    libgtk-3-dev \
    libsdl2-dev \
    libsdl2-image-dev \
    # Graphics
    libepoxy-dev \
    libgbm-dev \
    libdrm-dev \
    # Pixel manipulation
    libpixman-1-dev \
    # Device tree
    libfdt-dev \
    # Compression
    zlib1g-dev \
    liblzma-dev \
    liblz4-dev \
    libzstd-dev \
    # Misc QEMU deps
    libaio-dev \
    libcap-ng-dev \
    libnuma-dev \
    libusb-1.0-0-dev \
    libncurses-dev \
    libcurl4-gnutls-dev \
    libssh-dev \
    libpulse-dev \
    # Tools
    git \
    screen \
    flex \
    bison \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace/sim/MINI404
