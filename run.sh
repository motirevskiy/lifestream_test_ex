#!/bin/bash

./server/server &
SERVER_PID=$!

sleep 1

./client/client client/test_data/filenames.txt

echo "Stopping server"

kill $SERVER_PID