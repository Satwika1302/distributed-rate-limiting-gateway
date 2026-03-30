// High-Performance C++ API Gateway Core
// This is the main entry point for the asynchronous API Gateway.
// It handles the middleware pipeline, routing, and HTTP proxying.

#include <boost/asio.hpp>

#include <boost/beast.hpp>
#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include "Config.hpp"
#include "JwtAuth.hpp"
#include "RateLimiter.hpp"
#include "KafkaLogger.hpp"
#include <jwt-cpp/jwt.h>


namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

// GLOBAL PROMETHEUS METRICS (Thread-Safe)
std::atomic<uint64_t> metric_requests_total{0};
std::atomic<uint64_t> metric_429_total{0};
std::atomic<uint64_t> metric_401_total{0};
std::atomic<uint64_t> metric_404_total{0};

// Handle an individual HTTP request concurrently
void handle_request(tcp::socket socket, GatewayConfig* config, JwtAuth* auth, RateLimiter* limiter, KafkaLogger* logger) {
    auto start_time = std::chrono::high_resolution_clock::now();
    int final_status = 200;
    std::string path = "unknown";
    std::string method = "unknown";

    try {
        beast::flat_buffer buffer;
        http::request<http::string_body> req;
        http::read(socket, buffer, req);
        
        path = std::string(req.target());
        method = std::string(req.method_string());
        
        // Exclude /metrics from logging completely so it doesn't spam Kafka
        if (path != "/metrics") {
            std::cout << "\n[Incoming] " << method << " " << path << "\n";
            metric_requests_total++;
        }

        http::response<http::string_body> res;
        res.version(req.version());
        res.set(http::field::server, "CppGateway/1.0");
        res.set(http::field::content_type, "application/json");

        // GLOBAL CORS FOR REACT DASHBOARD
        res.set(http::field::access_control_allow_origin, "*");

        // HANDLE PREFLIGHT (OPTIONS) REQUESTS
        if (req.method() == http::verb::options) {
            res.result(http::status::no_content);
            res.set(http::field::access_control_allow_methods, "GET, POST, OPTIONS");
            res.set(http::field::access_control_allow_headers, "Authorization, Content-Type");
            res.prepare_payload();
            http::write(socket, res);
            boost::system::error_code ec;
            socket.shutdown(tcp::socket::shutdown_send, ec);
            return;
        }

        // PHASE 4: PROMETHEUS METRICS ENDPOINT (Bypasses Auth)
        if (path == "/metrics") {
            std::string prom = "# HELP gateway_requests_total Total number of HTTP requests\n";
            prom += "# TYPE gateway_requests_total counter\n";
            prom += "gateway_requests_total " + std::to_string(metric_requests_total.load()) + "\n";
            
            prom += "# HELP gateway_401_total Unauthorized Requests\n";
            prom += "# TYPE gateway_401_total counter\n";
            prom += "gateway_401_total " + std::to_string(metric_401_total.load()) + "\n";
            
            prom += "# HELP gateway_429_total Rate Limited Requests\n";
            prom += "# TYPE gateway_429_total counter\n";
            prom += "gateway_429_total " + std::to_string(metric_429_total.load()) + "\n";

            prom += "# HELP gateway_404_total Route Not Found\n";
            prom += "# TYPE gateway_404_total counter\n";
            prom += "gateway_404_total " + std::to_string(metric_404_total.load()) + "\n";

            res.result(http::status::ok);
            res.set(http::field::content_type, "text/plain");
            res.body() = prom;
            res.prepare_payload();
            http::write(socket, res);
            boost::system::error_code ec;
            socket.shutdown(tcp::socket::shutdown_send, ec);
            return; // EXIT EARLY, NO LOGGING
        }

        // MIDDLEWARE 1: JWT AUTHENTICATION
        std::string auth_header = std::string(req[http::field::authorization]);
        std::string token = JwtAuth::extract_token(auth_header);

        if (!auth->validate_token(token)) {
            metric_401_total++;
            std::cout << "--> 401 Unauthorized (JWT Blocked)\n";
            res.result(http::status::unauthorized);
            final_status = 401;
            res.body() = "{\"error\": \"Unauthorized: Invalid or Missing JWT\"}\n";
            res.prepare_payload();
            http::write(socket, res);
            boost::system::error_code ec;
            socket.shutdown(tcp::socket::shutdown_send, ec);
        }
        else {
            // IDENTITY PROPAGATION: Strip Authorization, inject X-Headers
            try {
                auto decoded = jwt::decode(token);
                if (decoded.has_payload_claim("iss")) {
                    req.set("X-User-ID", decoded.get_payload_claim("iss").as_string());
                    req.set("X-User-Role", "authenticated-user");
                }
            } catch(...) {}
            req.erase(http::field::authorization);
            
            // MIDDLEWARE 2: REDIS RATE LIMITING (Token Bucket)
            std::string client_ip = "127.0.0.1";
            
            if (!limiter->allow_request(client_ip)) {
                metric_429_total++;
                std::cout << "--> 429 Too Many Requests (Redis Rate Limit Blocked!)\n";
                res.result(http::status::too_many_requests);
                final_status = 429;
                res.body() = "{\"error\": \"Too Many Requests. Rate limit exceeded.\"}\n";
                res.prepare_payload();
                http::write(socket, res);
                boost::system::error_code ec;
                socket.shutdown(tcp::socket::shutdown_send, ec);
            }
            else {
                // MIDDLEWARE 3: DYNAMIC ROUTING & ROUND-ROBIN
                RouteConfig* route = config->match_route(path);
                
                if (route) {
                    std::string backend_instance = route->get_next_instance();
                    std::cout << "--> Proxying to " << route->service_name << " at " << backend_instance << "\n";
                    
                    try {
                        size_t colon_pos = backend_instance.find(':');
                        std::string host = backend_instance.substr(0, colon_pos);
                        std::string port_str = backend_instance.substr(colon_pos + 1);

                        asio::io_context proxy_ioc;
                        tcp::resolver resolver(proxy_ioc);
                        beast::tcp_stream proxy_stream(proxy_ioc);
                        
                        auto const results = resolver.resolve(host, port_str);
                        proxy_stream.connect(results);

                        // Forward the EXACT incoming request to the backend!
                        req.set(http::field::host, host);
                        http::write(proxy_stream, req);

                        // Read the EXACT response from the backend!
                        beast::flat_buffer proxy_buffer;
                        http::response<http::dynamic_body> proxy_res;
                        http::read(proxy_stream, proxy_buffer, proxy_res);
                        
                        // Stream it backwards entirely transparently
                        http::write(socket, proxy_res);
                        
                        final_status = proxy_res.result_int();
                        
                        // Close backend socket
                        beast::error_code proxy_ec;
                        proxy_stream.socket().shutdown(tcp::socket::shutdown_both, proxy_ec);
                        
                    } catch (std::exception const& e) {
                        std::cerr << "--> 502 Bad Gateway: " << e.what() << "\n";
                        res.result(http::status::bad_gateway);
                        final_status = 502;
                        res.body() = "{\"error\": \"502 Bad Gateway: Target Backend is unreachable.\"}\n";
                        res.prepare_payload();
                        http::write(socket, res);
                    }
                    
                    boost::system::error_code ec;
                    socket.shutdown(tcp::socket::shutdown_send, ec);
                } else {
                    metric_404_total++;
                    std::cout << "--> 404 Route Not Found\n";
                    res.result(http::status::not_found);
                    final_status = 404;
                    res.body() = "{\"error\": \"Route not found\"}\n";
                }
                
                res.prepare_payload();
                http::write(socket, res);
                
                boost::system::error_code ec;
                socket.shutdown(tcp::socket::shutdown_send, ec);
            }
        }
    } catch (std::exception const& e) {
        std::cerr << "Error handling request: " << e.what() << "\n";
        final_status = 500;
    }

    // FIRE OBSERVERS! 
    // This logs the latency and status directly into Kafka without blocking the socket which we cleanly shut down above.
    auto end_time = std::chrono::high_resolution_clock::now();
    double latency = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    logger->log_request(method, path, final_status, latency);
}

int main() {
    try {
        std::cout << "Initializing High-Performance C++ API Gateway...\n";

        // 1. Load Custom Config
        GatewayConfig config;
        if (!config.load("../config.json")) {
            std::cerr << "Failed to load config.json.\n";
            return 1;
        }

        // 2. Initialize Middlewares
        JwtAuth auth("SUPER_SECRET_MICROSOFT_KEY");
        
        // Use Environment Variables for Docker network compatibility!
        const char* redis_env = std::getenv("REDIS_URL");
        std::string redis_url = redis_env ? redis_env : "tcp://127.0.0.1:6379";
        RateLimiter limiter(redis_url, config.rate_limiter.default_burst, config.rate_limiter.default_refill_rate);
        
        // 3. Initialize Async Observability (Kafka port 9092)
        const char* kafka_env = std::getenv("KAFKA_BROKERS");
        std::string kafka_brokers = kafka_env ? kafka_env : "127.0.0.1:9092";
        KafkaLogger logger(kafka_brokers, "gateway-logs");
        std::cout << "Kafka Asynchronous Logger Engine initialized.\n";

        // Setup Networking
        asio::io_context io_context;
        tcp::acceptor acceptor(io_context, {tcp::v4(), 8080});
        
        std::cout << "\nGateway is online and listening on port 8080\n";
        std::cout << "Prometheus metrics available at /metrics\n";

        // Create a thread pool and keep it alive with a work guard
        auto work_guard = asio::make_work_guard(io_context);
        unsigned int num_threads = std::max(1u, std::thread::hardware_concurrency());
        std::vector<std::thread> thread_pool;
        for (unsigned int i = 0; i < num_threads; ++i) {
            thread_pool.emplace_back([&io_context] {
                try {
                    io_context.run();
                } catch (const std::exception& e) {
                    std::cerr << "Worker Thread Error: " << e.what() << "\n";
                }
            });
        }

        while (true) {
            auto socket = std::make_shared<tcp::socket>(io_context);
            acceptor.accept(*socket);
            
            // Dispatch to the thread pool for non-blocking execution
            asio::post(io_context, [socket, &config, &auth, &limiter, &logger]() mutable {
                try {
                    handle_request(std::move(*socket), &config, &auth, &limiter, &logger);
                } catch (const std::exception& e) {
                    std::cerr << "Request Handler Error: " << e.what() << "\n";
                }
            });
        }
        
        // Join threads (though we loop forever)
        for (auto& t : thread_pool) t.join();
    } catch (std::exception const& e) {
        std::cerr << "\nFatal Gateway Error: " << e.what() << "\n";
    }
}
