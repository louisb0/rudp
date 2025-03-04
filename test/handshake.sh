#!/bin/bash

# Temporary, until we log events internally and can do a proper test.

../build/rudp_server &
SERVER_PID=$!

sleep 1

../build/rudp_client &
CLIENT_PID=$!

cleanup() {
  kill $CLIENT_PID 2>/dev/null
  kill $SERVER_PID 2>/dev/null
  exit 0
}

trap cleanup SIGINT SIGTERM EXIT
wait $SERVER_PID
