# Qemu Testing

## Network

Currently the framework uses a tap device for the network interface on the
qemu. You can use the script at `scripts/setup_vm_network.sh` to setup the
network on your host machine, then use `scripts/clean_vm_network.sh` to 
remove it.


# Local integration testing

## Running the tests

To run all tests, simply run `bazel test --config=host <test target>`


# Creating Tests

Please look at `tests/integration/smoke` for a basic test case example.


