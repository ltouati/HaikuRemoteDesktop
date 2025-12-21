# Use the official Haiku cross-compiler image
FROM haiku/cross-compiler:x86_64-r1beta4

# 1. Update the base Linux environment
USER root
RUN apt-get update && apt-get install -y \
    wget \
    cmake \
    ninja-build \
    nodejs \
    npm \
    wget \
    cmake \
    ninja-build \
    nodejs \
    npm \
    git \
    unzip \
    && rm -rf /var/lib/apt/lists/*

# 2. Install Haiku Dependencies (OpenSSL, Libvpx, Protobuf)
# We download the exact versions found in the HaikuPorts repository.
WORKDIR /tmp
RUN mkdir -p /boot/system

# Download Base + Devel packages
# We include the 'haiku' base package to provide system data (licenses) so 'package create' works.
RUN wget https://eu.hpkg.haiku-os.org/haiku/master/x86_64/current/packages/haiku-r1~beta5_hrev59237-1-x86_64.hpkg \
    && wget https://eu.hpkg.haiku-os.org/haikuports/master/x86_64/current/packages/libvpx-1.13.1-1-x86_64.hpkg \
    && wget https://eu.hpkg.haiku-os.org/haikuports/master/x86_64/current/packages/libvpx_devel-1.13.1-1-x86_64.hpkg \
    && wget https://eu.hpkg.haiku-os.org/haikuports/master/x86_64/current/packages/openssl3-3.5.4-1-x86_64.hpkg \
    && wget https://eu.hpkg.haiku-os.org/haikuports/master/x86_64/current/packages/openssl3_devel-3.5.4-1-x86_64.hpkg \
    && wget https://eu.hpkg.haiku-os.org/haikuports/master/x86_64/current/packages/protobuf-3.20.1-1-x86_64.hpkg \
    && wget https://eu.hpkg.haiku-os.org/haikuports/master/x86_64/current/packages/protobuf_devel-3.20.1-1-x86_64.hpkg \
    && wget https://eu.hpkg.haiku-os.org/haikuports/master/x86_64/current/packages/x264-20220222-1-x86_64.hpkg \
    && wget https://eu.hpkg.haiku-os.org/haikuports/master/x86_64/current/packages/x264_devel-20220222-1-x86_64.hpkg

# Extract them to /boot/system
RUN package extract -C /boot/system haiku-r1~beta5_hrev59237-1-x86_64.hpkg \
    && package extract -C /boot/system libvpx-1.13.1-1-x86_64.hpkg \
    && package extract -C /boot/system libvpx_devel-1.13.1-1-x86_64.hpkg \
    && package extract -C /boot/system openssl3-3.5.4-1-x86_64.hpkg \
    && package extract -C /boot/system openssl3_devel-3.5.4-1-x86_64.hpkg \
    && package extract -C /boot/system protobuf-3.20.1-1-x86_64.hpkg \
    && package extract -C /boot/system protobuf_devel-3.20.1-1-x86_64.hpkg \
    && package extract -C /boot/system x264-20220222-1-x86_64.hpkg \
    && package extract -C /boot/system x264_devel-20220222-1-x86_64.hpkg \
    && rm *.hpkg

# Create symlink /system -> /boot/system (package tool workaround)
RUN ln -s /boot/system /system

# 3. Install Protobuf Compiler (Host - Linux)
# Must match the version of libprotobuf on Haiku (3.20.1) to avoid header mismatch errors.
RUN wget https://github.com/protocolbuffers/protobuf/releases/download/v3.20.1/protoc-3.20.1-linux-x86_64.zip \
    && unzip protoc-3.20.1-linux-x86_64.zip -d /usr/local \
    && rm protoc-3.20.1-linux-x86_64.zip

# 4. Setup Toolchain
COPY Toolchain.cmake /toolchain.cmake
ENV CMAKE_TOOLCHAIN_FILE=/toolchain.cmake
ENV HAIKU_INSTALL_DIR=/boot/system
RUN mkdir -p /tmp/haiku/data/system
# 4. Copy source code
WORKDIR /work
COPY . .

# 5. Define build command
CMD ["sh", "-c", "mkdir -p build && cd build && cmake .. -DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE -GNinja -DProtobuf_PROTOC_EXECUTABLE=/usr/local/bin/protoc -DRELEASE_MODE=ON && ninja package_haiku"]
