#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$root/build/sup26ad-regression"
cd "$root/build/sup26ad-regression"
g++ -std=c++14 -O0 -ffunction-sections -fdata-sections -I"$root/include/laserWeldSDK" -I"$root/include/laserWeldSDK/brand" -I"$root/include/laserWeldSDK/common" -I"$root/third_party/cr_sdk/include" "$root/tests/sup26ad_protocol_test.cpp" "$root"/src/*.cpp "$root"/src/brand/*.cpp "$root"/src/common/*.cpp "$root/include/laserWeldSDK/brand/lasermodbusrtubuilder.cpp" -Wl,--gc-sections -pthread -o chain-test
./chain-test
