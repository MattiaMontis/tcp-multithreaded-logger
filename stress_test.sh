#!/bin/bash
# Lancia 100 client in parallelo e attende la loro conclusione, 
# chmod +x test/stress_test.sh
for i in {1..100}; do
    ../client $i 1234 &
done
wait
echo "Stress test avviato."
