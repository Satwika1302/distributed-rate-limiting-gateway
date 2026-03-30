#!/bin/bash
# Microservices Orchestration Script
# This script launches multiple instances of User and Order services.

# Start User Service instances
python3 user_service.py 8081 &
PID1=$!
python3 user_service.py 8082 &
PID2=$!

# Start Order Service instances
python3 order_service.py 9091 &
PID3=$!
python3 order_service.py 9092 &
PID4=$!

trap "echo -e '\nShutting down all Mock Microservices...'; kill $PID1 $PID2 $PID3 $PID4; exit" SIGINT SIGTERM

echo ""
echo "Mock Microservices Running"
echo "--------------------------"
echo "User Service:  8081, 8082"
echo "Order Service: 9091, 9092"
echo "--------------------------"
echo "Press Ctrl+C to stop all services."
echo ""

wait
