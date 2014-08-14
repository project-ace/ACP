#./acpch_test0  0 2 100 1024 44256 44256 localhost > log/test0-00.txt  2>&1 &
#./acpch_test0  1 2 100 1024 44257 44256 localhost > log/test0-01.txt  2>&1 &
ssh pc01 ./acpch_test0  0 2 1024 44256 44257 192.168.1.1 &
ssh pc02 ./acpch_test0  1 2 1024 44257 44256 192.168.1.1 

