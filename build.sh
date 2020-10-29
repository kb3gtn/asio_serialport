#!/bin/bash
# simple build script for linux targets.
# assumes you have a C++11 capble compiler.
g++ ./tests/AsioSerialTest.cpp -I./inc -std=c++11 -lpthread -o AsioSerialTest

