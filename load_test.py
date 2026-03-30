# Asynchronous Load Testing Script
# This script uses aiohttp to simulate high-concurrency traffic.
# Used for benchmarking the throughput of the C++ API Gateway.

import asyncio
import aiohttp
import time
import json

# Configuration settings
URL = "http://localhost:8080/api/users"
JWT = "eyJhbGciOiAiSFMyNTYiLCAidHlwIjogIkpXVCJ9.eyJpc3MiOiAiY3BwLWdhdGV3YXkifQ.mLFnvY5z3qRWvD8roPuCzFEBLgXqU6oKaoYHhfTKkcI"
TOTAL_REQUESTS = 10000
CONCURRENCY = 100

async def fetch(session, i):
    headers = {"Authorization": f"Bearer {JWT}"}
    try:
        async with session.get(URL, headers=headers) as response:
            status = response.status
            # Read small amount of data to ensure connection is ready for next
            await response.read()
            return status
    except Exception as e:
        return f"Error: {str(e)}"

async def main():
    print(f"Starting Load Test on {URL}")
    print(f"Config: {TOTAL_REQUESTS} total requests, {CONCURRENCY} concurrent workers")
    print("-" * 50)

    start_time = time.perf_counter()
    
    connector = aiohttp.TCPConnector(limit=CONCURRENCY)
    async with aiohttp.ClientSession(connector=connector) as session:
        # Phase 1: Valid Requests (demonstrates 200 and 429)
        tasks = [fetch(session, i) for i in range(TOTAL_REQUESTS)]
        
        # Phase 2: Intentional Unauthorized Requests (demonstrates 401)
        async def fetch_bad(session):
            headers = {"Authorization": "Bearer BAD_TOKEN"}
            async with session.get(URL, headers=headers) as response:
                return response.status

        tasks += [fetch_bad(session) for _ in range(100)]
        
        results = await asyncio.gather(*tasks)

    end_time = time.perf_counter()
    duration = end_time - start_time

    # Statistics
    stats = {}
    for r in results:
        code = r if isinstance(r, int) else "Error"
        stats[code] = stats.get(code, 0) + 1

    print("\nLoad Test Complete")
    print(f"Duration: {duration:.2f} seconds")
    print(f"Throughput: {TOTAL_REQUESTS / duration:.2f} requests/sec")
    print("\nResponse Status Codes:")
    for code, count in sorted(stats.items(), key=lambda x: str(x[0])):
        status_text = ""
        if code == 200: status_text = " (OK)"
        elif code == 429: status_text = " (Rate Limited)"
        elif code == 401: status_text = " (Unauthorized)"
        print(f"  - {code}{status_text}: {count}")

    print("-" * 50)
    print("Tip: Watch the React Dashboard during the test to see real-time spikes")

if __name__ == "__main__":
    asyncio.run(main())
