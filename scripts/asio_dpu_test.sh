#!/bin/bash

infile=$1
outfile=$2
transferSize=$3
transferCount=$4

testError=0

rm -f $outfile
../src/asio_from_dpu -o $outfile -s $transferSize -c $transferCount &

../src/asio_to_dpu -i $infile -s $transferSize -c $transferCount &

# Wait for the current transactions to complete
echo "Info: Wait the for current transactions to complete."
wait

# Verify that the written data matches the read data.
echo "Info: Checking data integrity."
cmp $outfile $infile
returnVal=$?
if [ ! $returnVal == 0 ]; then
    echo "Error: The data written did not match the data that was read."
    testError=1
else
    echo "Info: Data check passed for c2h and h2c channel"
fi

# Exit with an error code if an error was found during testing
if [ $testError -eq 1 ]; then
  echo "Error: Test completed with Errors."
  exit 1
fi

# Report all tests passed and exit
echo "Info: All PCIe DMA streaming tests passed."
exit 0
