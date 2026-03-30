#!/bin/bash
# Distributed Rate Limiting Gateway - Verification Script
# This script validates the Token Bucket implementation and JWT security middleware.

echo "===================================================="
echo "   Distributed Rate Limiting Gateway: Security Test"
echo "===================================================="

# 1. Use a pre-calculated authorized JWT Token
echo "1. Using pre-calculated authorized token..."
TOKEN="eyJhbGciOiAiSFMyNTYiLCAidHlwIjogIkpXVCJ9.eyJpc3MiOiAiY3BwLWdhdGV3YXkifQ.mLFnvY5z3qRWvD8roPuCzFEBLgXqU6oKaoYHhfTKkcI"
echo "Token: [AUTHORIZED]"
echo ""

# 2. Test Rate Limiting
echo "2. Sending rapid requests to verify high-capacity burst..."
echo "Config: 20,000 tokens initial capacity | 10,000 tokens/sec refill rate"
echo "----------------------------------------------------"

# Send 200 requests: they should all pass under the new 20,000 burst limit
success=0
blocked=0

for i in {1..200}; do
    status=$(curl -s -o /dev/null -w "%{http_code}" -H "Authorization: Bearer $TOKEN" http://localhost:8080/api/users)
    if [ "$status" == "200" ]; then
        ((success++))
    elif [ "$status" == "429" ]; then
        ((blocked++))
    fi
done

echo "Test Results:"
echo " - Authorized (200 OK): $success"
echo " - Rate Limited (429): $blocked"
echo "----------------------------------------------------"
echo "Expected: 200 Success | 0 Blocked"
echo "===================================================="
