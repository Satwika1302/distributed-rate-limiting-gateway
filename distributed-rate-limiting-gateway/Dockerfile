FROM ubuntu:22.04

# Avoid tzdata interactive prompt during docker build
ENV DEBIAN_FRONTEND=noninteractive

# Install all C++ build tools and external C libraries via the official Ubuntu repo.
# This avoids building heavy dependencies like Boost from source which takes 20+ minutes.
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libssl-dev \
    libboost-all-dev \
    libhiredis-dev \
    librdkafka-dev \
    pkg-config \
    git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the source code and CMake build system
COPY CMakeLists.txt ./
COPY include/ ./include/
COPY src/ ./src/

# Copy the config.json inside so the binary finds it!
COPY config.json ./

# Forced rebuild trigger: 2026-03-30-17-10
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . -j$(nproc)

# Expose the API Gateway HTTP port
EXPOSE 8080

# When the container starts, launch the gateway
# It will look for config.json in the parent directory as coded in main.cpp,
# so we run it from the build directory just like we do locally.
WORKDIR /app/build
CMD ["./gateway"]
