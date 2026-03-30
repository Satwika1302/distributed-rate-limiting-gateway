cmake_minimum_required(VERSION 3.15)
project(distributed_rate_limiting_gateway CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Boost (system and thread are often required for Asio)
# Find Boost
find_package(Boost REQUIRED COMPONENTS system thread)

# Include directories
include_directories(${Boost_INCLUDE_DIRS})
include_directories(include)

# Fetch nlohmann/json (Used by both Gateway and jwt-cpp)
include(FetchContent)
if(NOT TARGET nlohmann_json)
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Fetch jwt-cpp for JWT Validation
FetchContent_Declare(
    jwt_cpp
    URL https://github.com/Thalhammer/jwt-cpp/archive/refs/tags/v0.7.0.tar.gz
)
FetchContent_MakeAvailable(jwt_cpp)

# Find OpenSSL (ensure it works on Mac arm64 with Homebrew)
set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl@3")
find_package(OpenSSL REQUIRED)

# Find Homebrew's hiredis!
find_path(HIREDIS_HEADER hiredis/hiredis.h PATHS /opt/homebrew/include /usr/local/include)
find_library(HIREDIS_LIB hiredis PATHS /opt/homebrew/lib /usr/local/lib)

# Find Homebrew's librdkafka!
find_path(LIBRDKAFKA_HEADER librdkafka/rdkafkacpp.h PATHS /opt/homebrew/include /usr/local/include)
find_library(LIBRDKAFKA_LIB "rdkafka++" PATHS /opt/homebrew/lib /usr/local/lib)

# Fetch redis-plus-plus (C++ wrapper)
set(REDIS_PLUS_PLUS_BUILD_TEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    redis_plus_plus
    GIT_REPOSITORY https://github.com/sewenew/redis-plus-plus.git
    GIT_TAG 1.3.12
)
FetchContent_MakeAvailable(redis_plus_plus)

# Add executable
add_executable(gateway src/main.cpp src/Config.cpp src/JwtAuth.cpp src/RateLimiter.cpp src/KafkaLogger.cpp)

# Link libraries
target_link_libraries(gateway ${Boost_LIBRARIES} pthread nlohmann_json::nlohmann_json jwt-cpp::jwt-cpp OpenSSL::SSL OpenSSL::Crypto ${HIREDIS_LIB} redis++ ${LIBRDKAFKA_LIB})
