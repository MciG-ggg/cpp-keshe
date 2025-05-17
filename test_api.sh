#!/bin/bash

# Base URL
BASE_URL="http://localhost:8080"

echo "Testing Parking Management API..."

# Test 1: Add a vehicle
echo -e "\n1. Adding a vehicle..."
curl -X POST "${BASE_URL}/api/vehicle" \
     -H "Content-Type: application/json" \
     -H "Accept: application/json" \
     -d '{
       "plate": "苏A12345",
       "type": "小型"
     }' \
     -v

sleep 1

# Test 2: Query vehicle status
echo -e "\n\n2. Querying vehicle status..."
curl -X GET "${BASE_URL}/api/vehicle/苏A12345" \
     -H "Accept: application/json" \
     -v

sleep 1

# Test 3: Get parking lot status
echo -e "\n\n3. Getting parking lot status..."
curl -X GET "${BASE_URL}/api/status" \
     -H "Accept: application/json" \
     -v

sleep 1

# Test 4: Update parking rates
echo -e "\n\n4. Updating parking rates..."
curl -X PUT "${BASE_URL}/api/rate" \
     -H "Content-Type: application/json" \
     -H "Accept: application/json" \
     -d '{
       "smallRate": 6.0,
       "largeRate": 10.0
     }' \
     -v

sleep 1

# Test 5: Remove vehicle
echo -e "\n\n5. Removing vehicle..."
curl -X DELETE "${BASE_URL}/api/vehicle/苏A12345" \
     -H "Accept: application/json" \
     -v

sleep 1

# Test 6: Get parking history
echo -e "\n\n6. Getting parking history..."
curl -X GET "${BASE_URL}/api/history" \
     -H "Accept: application/json" \
     -v

echo -e "\n\nAPI testing completed."