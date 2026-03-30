# Distributed Rate Limiting Gateway

This is a high-performance API Gateway built with C++20. It acts as a secure and fast entry point for your microservices, specializing in distributed traffic control and real-time observability.

## Key Features
*   **High Performance**: Benchmarked at **4,000+ Requests Per Second** (Docker on Mac) with a persistent Boost.Asio Thread Pool and atomic Redis-Lua state management.
*   **Edge-Native State**: Designed for sub-millisecond throughput using **Redis** for global rate-limit state and **Apache Kafka** for high-volume identity audit logs, bypassing traditional RDBMS bottlenecks.
*   **Distributed Rate Limiting**: Manages global request limits across multiple gateway nodes using a shared Redis state.
*   **Request Security**: Validates JWT tokens at the edge and injects verified user identity into backend request headers.
*   **Real-Time Monitoring**: Includes a React-based dashboard for live tracking of traffic, success rates, and rate-limit events.
*   **Background Logging**: Uses an asynchronous Kafka pipeline to record activity without slowing down user requests.
*   **Identity Propagation**: Injects verified `X-User-ID` headers into backend requests, enabling seamless downstream authorization.

## Performance & Benchmarking
We verified the system's performance using an asynchronous stress-testing suite:
*   **Methodology**: Asynchronous load tester (Python `aiohttp`) with 50 concurrent workers.
*   **Results**: Verified peak throughput of **4,116 Requests Per Second (RPS)**.
*   **Overhead**: Sub-millisecond internal processing time added to each request.

## Rate Limiting Architecture
The system implements an enterprise-grade traffic shaping strategy:
*   **Algorithm**: Distributed **Token Bucket** algorithm (shared via Redis).
*   **Burst Capacity**: Pre-configured for a **1,000-request burst** to handle traffic spikes.
*   **Refill Rate**: 10 requests per second recovery following a burst.

### D. Verify Identity Propagation
You can witness the Gateway securely injecting user identities into backend services by watching the container logs:
```bash
# Watch backend audit logs
docker compose logs -f user-service
```
**Observation**: When you run `load_test.py`, you will see lines like:
`[AUDIT] Request authenticated for User: user123 | Handled by: user-service-8081`
This confirms the Gateway is successfully performing edge authentication and propagating state.

## 3. Observability Dashboard

## Tech Stack
*   **Language**: C++20 (Boost.Asio, Boost.Beast)
*   **Distributed State**: Redis
*   **Observability**: Prometheus, React, Apache Kafka
*   **Deployment**: Docker & Docker Compose

## Getting Started
Please refer to [walkthrough.md](./walkthrough.md) for step-by-step setup and benchmarking instructions.
