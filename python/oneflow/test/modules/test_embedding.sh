
export OPTIMIZER="sgd"
export FIXED_TABLE_BLOCK_SIZE=512
export TEST_OPTIMIZER=$OPTIMIZER"_test"
echo "testing model load: "$TEST_OPTIMIZER
path=/NVME0/guoran/unittest/$TEST_OPTIMIZER
cp /NVME0/guoran/unittest/$TEST_OPTIMIZER/0-1/index_cp /NVME0/guoran/unittest/$TEST_OPTIMIZER/0-1/index
python3 test_embedding_lookup_model_load.py 
python3 test_debug_model_load.py

export OPTIMIZER="momentum"
export FIXED_TABLE_BLOCK_SIZE=1024
export TEST_OPTIMIZER=$OPTIMIZER"_test"
echo "testing model load: "$TEST_OPTIMIZER
path=/NVME0/guoran/unittest/$TEST_OPTIMIZER
cp /NVME0/guoran/unittest/$TEST_OPTIMIZER/0-1/index_cp /NVME0/guoran/unittest/$TEST_OPTIMIZER/0-1/index
python3 test_embedding_lookup_model_load.py 
python3 test_debug_model_load.py

export OPTIMIZER="adam"
export FIXED_TABLE_BLOCK_SIZE=1536
export TEST_OPTIMIZER=$OPTIMIZER"_test"
echo "testing model load: "$TEST_OPTIMIZER
path=/NVME0/guoran/unittest/$TEST_OPTIMIZER
cp /NVME0/guoran/unittest/$TEST_OPTIMIZER/0-1/index_cp /NVME0/guoran/unittest/$TEST_OPTIMIZER/0-1/index
python3 test_embedding_lookup_model_load.py 
python3 test_debug_model_load.py