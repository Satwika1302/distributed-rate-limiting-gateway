#pragma once
#include <sw/redis++/redis++.h>
#include <string>
#include <memory>

class RateLimiter {
private:
    std::shared_ptr<sw::redis::Redis> redis_client;
    double capacity;
    double refill_rate; // tokens added per second

public:
    RateLimiter(const std::string& redis_url, double cap, double rate);
    
    // Checks if an IP is allowed under the Token Bucket limits
    bool allow_request(const std::string& ip_address);
};
