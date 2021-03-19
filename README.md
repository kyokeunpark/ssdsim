# ssdsim
SSD Simulator for CSC2233 Project @ University of Toronto

## Building

``` sh
mkdir build && cd build
cmake ../ && make
```

This will generate `simulator` and `test` binary within `build` directory.

## Writing Tests

We are using [googletest](https://github.com/google/googletest) to test
parts of the simulator. We can use `test` binary to run the test cases.
All the tests can be written within `test.cpp`. You can learn more about
writing test cases with googletest
[here](https://github.com/google/googletest/blob/master/docs/primer.md#simple-tests).
