# Distributed Rate Limiting Gateway

This is a high-performance API Gateway built with C++20. It acts as a secure and fast entry point for your microservices, specializing in distributed traffic control and real-time observability.

## Key Features
*   **High Performance**: Benchmarked at **4,000+ Requests Per Second** (Docker on Mac) with a persistent Boost.Asio Thread Pool and atomic Redis-Lua state management.
*   **Edge-Native State**: Designed for sub-millisecond throughput using **Redis** for global rate-limit state and **Apache Kafka** for high-volume identity audit logs, bypassing traditional RDBMS bottlenecks.
*   **Distributed Rate Limiting**: Manages global request limits across multiple gateway nodes using a shared Redis state.
*   **Request Security**: Validates JWT tokens at the edge and injects verified user identity into backend request headers.
*   **Real-Time Monitoring**: Includes a React-based dashboard for live tracking of traffic, success rates, and rate-limit events.
*   **Background Logging**: Uses an asynchronous Kafka pipeline to record activity without slowing down user requests.

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

## Tech Stack
*   **Language**: C++20 (Boost.Asio, Boost.Beast)
*   **Distributed State**: Redis
*   **Observability**: Prometheus, React, Apache Kafka
*   **Deployment**: Docker & Docker Compose

## Getting Started
Please refer to [walkthrough.md](./walkthrough.md) for step-by-step setup and benchmarking instructions.
