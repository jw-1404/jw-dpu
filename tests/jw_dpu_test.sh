#!/bin/bash

infile=$1
outfile=$2
transferSize=$3
transferCount=$4
waitTime=$5

testError=0

# Setup the DMA c2h channels to wait for incomming data from the h2c channels.
rm -f data/$outfile
../src/jw_from_device -e -d /dev/xdma0_c2h_0 -f $outfile -s $transferSize -c $transferCount -u $waitTime &

# Wait to make sure the DMA is ready to receive data.
sleep 1s

# Setup the DMA to write to the h2c channels. Data will be push out the h2c channel
# and then read back through the c2h channel and written to the output data file.
../src/jw_to_device -d /dev/xdma0_h2c_0 -f $infile -s $transferSize -c $transferCount -u $waitTime &

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
