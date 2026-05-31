# Build stage
FROM ubuntu:22.04 AS build

WORKDIR /app

# Install required build tools and dependencies
RUN apt-get update && apt-get install -y \
    git \
    cmake \
    g++ \
    curl \
    zip \
    unzip \
    tar \
    pkg-config \
    autoconf \
    autoconf-archive \
    automake \
    libtool \
    && rm -rf /var/lib/apt/lists/*

# Install vcpkg
RUN git clone https://github.com/Microsoft/vcpkg.git /opt/vcpkg
RUN /opt/vcpkg/bootstrap-vcpkg.sh
ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"

# Install dependencies
ENV VCPKG_MAX_CONCURRENCY=2
RUN vcpkg install gamenetworkingsockets:x64-linux ftxui:x64-linux sqlitecpp:x64-linux libsodium:x64-linux

# Copy source code
COPY . .

# Build the project
WORKDIR /app/VoiceChatServer
RUN cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake
RUN cmake --build build --config Release

# Runtime stage
FROM ubuntu:22.04 AS runtime

WORKDIR /app

# Create persistent data directory
RUN mkdir -p /app/data

# Copy the built binary
COPY --from=build /app/VoiceChatServer/build/VoiceChatServer /app/

# Copy all vcpkg-built shared libraries (avoids ABI mismatch with apt packages)
COPY --from=build /opt/vcpkg/installed/x64-linux/lib /usr/lib/
# Ensure dynamic linker can find the libraries
ENV LD_LIBRARY_PATH=/usr/lib

# Only system-level runtime deps not provided by vcpkg
# libssl3 is already included in ubuntu:22.04 base image

# Expose the port the server will listen on (UDP)
EXPOSE 27020/udp

# Run the server
CMD ["./VoiceChatServer"]