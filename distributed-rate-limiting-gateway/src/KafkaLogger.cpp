// Asynchronous Telemetry and Logging Engine
// Uses librdkafka to produce fire-and-forget logs to an Apache Kafka topic.
// Designed to minimize latency impact on the critical request path.

#include "KafkaLogger.hpp"

#include <nlohmann/json.hpp>
#include <ctime>

using json = nlohmann::json;

KafkaLogger::KafkaLogger(const std::string& brokers, const std::string& topic) : topic_name(topic) {
    std::string errstr;

    // Create configuration object for Kafka Producer
    RdKafka::Conf *conf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
    conf->set("bootstrap.servers", brokers, errstr);
    conf->set("client.id", "distributed-rate-limiting-gateway-logger", errstr);
    
    // Create producer instance
    producer.reset(RdKafka::Producer::create(conf, errstr));
    if (!producer) {
        std::cerr << "CRITICAL: Failed to create Kafka producer: " << errstr << "\n";
    } else {
        std::cout << "Successfully connected to Kafka Brokers at " << brokers << "\n";
    }
    
    delete conf;
}

KafkaLogger::~KafkaLogger() {
    if (producer) {
        // Wait at most 5 seconds for pending logs to be pushed to Kafka before shutting down
        producer->flush(5000);
    }
}

void KafkaLogger::log_request(const std::string& method, const std::string& path, 
                              int status_code, double latency_ms) {
    if (!producer) return;
    
    // Use nlohmann to structure the log securely
    json log_entry = {
        {"method", method},
        {"path", path},
        {"status_code", status_code},
        {"latency_ms", latency_ms},
        {"timestamp", std::time(nullptr)}
    };
    
    std::string payload = log_entry.dump();

    // The core of high-performance observability: Fire and Forget.
    // This pushes the message string memory to an isolated TCP thread running in librdkafka.
    RdKafka::ErrorCode err = producer->produce(
        topic_name,
        RdKafka::Topic::PARTITION_UA,
        RdKafka::Producer::RK_MSG_COPY, /* Copy payload since our string goes out of scope */
        const_cast<char *>(payload.c_str()), payload.size(),
        NULL, 0,
        0, /* Timestamp */
        NULL /* Delivery report opaque pointer */
    );

    if (err != RdKafka::ERR_NO_ERROR) {
        std::cerr << "% Failed to enqueue log message for Kafka: " << RdKafka::err2str(err) << "\n";
    } else {
        // Poll lightly to trigger delivery callbacks for prior messages
        producer->poll(0);
    }
}
