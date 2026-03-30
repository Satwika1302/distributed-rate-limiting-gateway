#pragma once
#include <librdkafka/rdkafkacpp.h>
#include <string>
#include <memory>
#include <iostream>

class KafkaLogger {
private:
    std::unique_ptr<RdKafka::Producer> producer;
    std::string topic_name;
    
public:
    KafkaLogger(const std::string& brokers, const std::string& topic);
    ~KafkaLogger();

    // Fire-and-forget asynchronous logging (does not block API threads)
    void log_request(const std::string& method, const std::string& path, 
                     int status_code, double latency_ms);
};
