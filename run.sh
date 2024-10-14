#!/bin/bash

repeat_count=5

./server/server &
SERVER_PID=$!

sleep 1

run_client() {
    local flag=$1
    local total_time=0

    for i in $(seq 1 $repeat_count)
    do
        echo "Run #$i with flag '$flag':"
        start_time=$(date +%s.%N)
        
        if [ "$flag" == "-m" ]; then
            ./client/client client/test_data/filenames.txt -m
        else
            ./client/client client/test_data/filenames.txt
        fi

        end_time=$(date +%s.%N)
        run_time=$(echo "$end_time - $start_time" | bc)
        
        total_time=$(echo "$total_time + $run_time" | bc)
    done

    average_time=$(echo "$total_time / $repeat_count" | bc -l)
    echo "Average time with flag '$flag': $average_time seconds"
}

run_client ""
run_client "-m"

# Остановка сервера
echo "Stopping server"
kill $SERVER_PID