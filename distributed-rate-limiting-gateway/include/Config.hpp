#pragma once

#include <string>
#include <vector>
#include <map>
#include <atomic>

// Represents a single destination rule from the config
struct RouteConfig {
    std::string path_prefix;
    std::string service_name;
    std::vector<std::string> instances;
    
    // An atomic counter to handle Round-Robin load balancing safely across threads
    std::atomic<size_t> current_instance_idx{0};

    // Custom constructors to fix std::atomic deleted move errors
    RouteConfig() = default;
    RouteConfig(RouteConfig&& other) noexcept 
        : path_prefix(std::move(other.path_prefix)),
          service_name(std::move(other.service_name)),
          instances(std::move(other.instances)),
          current_instance_idx(other.current_instance_idx.load(std::memory_order_relaxed)) {}

    // Get the next instance in the list (Thread-safe Round-Robin)
    std::string get_next_instance();
};

struct RateLimiterConfig {
    bool enabled = true;
    double default_burst = 100.0;
    double default_refill_rate = 10.0;
};

class GatewayConfig {
private:
    std::vector<RouteConfig> routes;

public:
    RateLimiterConfig rate_limiter;
    // Load config from a JSON file
    bool load(const std::string& filepath);
    
    // Find a matching route for a given request path
    RouteConfig* match_route(const std::string& request_path);
};
