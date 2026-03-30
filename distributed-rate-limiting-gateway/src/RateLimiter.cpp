// Distributed Rate Limiting Middleware
// Implements the Token Bucket algorithm using Redis for shared state.
// This ensures traffic limits are enforced across all gateway instances.

#include "RateLimiter.hpp"

#include <chrono>
#include <iostream>

using namespace sw::redis;

RateLimiter::RateLimiter(const std::string& redis_url, double cap, double rate)
    : capacity(cap), refill_rate(rate) {
    try {
        redis_client = std::make_shared<Redis>(redis_url);
        // Ping to test the connection on startup
        redis_client->ping();
        std::cout << "Successfully connected to Redis at " << redis_url << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "WARNING: Could not connect to Redis at startup: " << e.what() << std::endl;
        std::cerr << "Gateway will start in 'Fail-Open' mode for rate limiting." << std::endl;
    }
}

bool RateLimiter::allow_request(const std::string& ip_address) {
    std::string tokens_key = "rate_limit:tokens:" + ip_address;
    std::string time_key = "rate_limit:time:" + ip_address;

    // LUA SCRIPT: Atomic Token Bucket (Fastest production-grade method)
    // KEYS[1]: tokens_key, KEYS[2]: time_key
    // ARGV[1]: capacity, ARGV[2]: refill_rate, ARGV[3]: now_sec
    const char* LUA_SCRIPT = R"(
        local capacity = tonumber(ARGV[1])
        local refill_rate = tonumber(ARGV[2])
        local now = tonumber(ARGV[3])
        local ttl = 3600

        local last_tokens = tonumber(redis.call('get', KEYS[1]) or capacity)
        local last_time = tonumber(redis.call('get', KEYS[2]) or now)

        local elapsed = math.max(0, now - last_time)
        local tokens = math.min(capacity, last_tokens + (elapsed * refill_rate))

        if tokens >= 1 then
            tokens = tokens - 1
            redis.call('setex', KEYS[1], ttl, tokens)
            redis.call('setex', KEYS[2], ttl, now)
            return 1
        else
            return 0
        end
    )";

    try {
        auto now = std::chrono::system_clock::now();
        double now_sec = std::chrono::duration<double>(now.time_since_epoch()).count();

        auto result = redis_client->eval<long long>(LUA_SCRIPT, 
            {tokens_key, time_key}, 
            {std::to_string(capacity), std::to_string(refill_rate), std::to_string(now_sec)});

        return result == 1;
    } catch (const std::exception &e) {
        std::cerr << "Redis/Lua Error: " << e.what() << "\n";
        return true; // Fail open
    }
}
