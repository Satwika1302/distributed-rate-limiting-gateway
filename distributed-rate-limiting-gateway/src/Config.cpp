#include "Config.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

std::string RouteConfig::get_next_instance() {
    if (instances.empty()) return "";

    // Safely increment and wrap around using modulo
    size_t idx = current_instance_idx.fetch_add(1, std::memory_order_relaxed);
    return instances[idx % instances.size()];
}

bool GatewayConfig::load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open config file: " << filepath << "\n";
        return false;
    }

    try {
        json j;
        file >> j;

        // Parse Routes
        if (j.contains("routes")) {
            for (const auto& route : j["routes"]) {
                RouteConfig rc;
                rc.path_prefix = route.value("path_prefix", "");
                rc.service_name = route.value("service_name", "");
                if (route.contains("instances")) {
                    for (const auto& inst : route["instances"]) {
                        rc.instances.push_back(inst);
                    }
                }
                routes.push_back(std::move(rc));
            }
        }
        
        // Parse Rate Limiter settings
        if (j.contains("rate_limiter")) {
            auto rl = j["rate_limiter"];
            rate_limiter.enabled = rl.value("enabled", true);
            rate_limiter.default_burst = rl.value("default_burst", 100.0);
            rate_limiter.default_refill_rate = rl.value("default_refill_rate", 10.0);
        }

        std::cout << "Loaded " << routes.size() << " routes and Rate Limiter config.\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "JSON Parsing Error: " << e.what() << "\n";
        return false;
    }
}

RouteConfig* GatewayConfig::match_route(const std::string& request_path) {
    for (auto& route : routes) {
        // Simple prefix matching
        if (request_path.find(route.path_prefix) == 0) {
            return &route;
        }
    }
    return nullptr; // No match found
}
