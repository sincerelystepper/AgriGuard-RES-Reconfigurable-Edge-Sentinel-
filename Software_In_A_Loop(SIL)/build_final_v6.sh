#!/bin/bash

echo "=============================================="
echo " Building Agriguard - v6 (Linker fixes)"
echo "=============================================="

rm -f renode_bus.h renode_bus.cpp renode_agent.h renode_agent.cpp Agriguard.exe

# Fix corrupted headers
if [ -f src/renode_bus.h ] && head -1 src/renode_bus.h | grep -q "Check if"; then
    cp src/IntegrationLibrary/src/renode_bus.h src/renode_bus.h
    sed -i 's|#include "src/|#include "|g' src/renode_bus.h
    sed -i 's|#include "../../../../Infrastructure/src/Emulator/Cores/renode/include/renode_imports.h"|// Not available|g' src/renode_bus.h
fi

# Fix EXTERNAL_AS macros
for file in src/renode_bus.cpp; do
    if [ -f "$file" ]; then
        sed -i 's|EXTERNAL_AS(void, HandleMainMessage, handleMainMessage, voidptr);|extern "C" void HandleMainMessage(void* ptr) { handleMainMessage(ptr); }|g' "$file"
        sed -i 's|EXTERNAL_AS(void, HandleSenderMessage, handleSenderMessage, voidptr);|extern "C" void HandleSenderMessage(void* ptr) { handleSenderMessage(ptr); }|g' "$file"
        sed -i 's|EXTERNAL_AS(void, Receive, receive, voidptr);|extern "C" void Receive(void* ptr) { receive(ptr); }|g' "$file"
    fi
done

# Fix include paths
find . -type f \( -name "*.cpp" -o -name "*.h" \) -exec sed -i 's|#include "src/|#include "|g' {} \;

# Setup
mkdir -p hdl/includes src/communication
cp src/IntegrationLibrary/hdl/includes/renode_action_enumerators.svh hdl/includes/ 2>/dev/null
cp src/IntegrationLibrary/src/communication/communication_channel.h src/communication/ 2>/dev/null

# Find additional Verilator libraries
VERILATOR_DIR="C:/msys64/ucrt64/share/verilator/include"
VERILATOR_THREADS=""
if [ -f "$VERILATOR_DIR/verilated_threads.cpp" ]; then
    VERILATOR_THREADS="$VERILATOR_DIR/verilated_threads.cpp"
    echo "✓ Found verilated_threads.cpp"
fi

echo "✓ Compiling and linking..."

g++ -Wall -Os -std=c++17 \
  -include cstring \
  -include string.h \
  -I. \
  -Iobj_dir \
  -Isrc \
  -Isrc/peripherals \
  -Isrc/buses \
  -Isrc/communication \
  -Isrc/IntegrationLibrary/src \
  -Isrc/IntegrationLibrary/src/communication \
  -Isrc/IntegrationLibrary/src/buses \
  -Isrc/IntegrationLibrary/src/peripherals \
  -Isrc/IntegrationLibrary/libs/socket-cpp/Socket \
  -Ihdl/includes \
  -I"$VERILATOR_DIR" \
  -I"$VERILATOR_DIR/vltstd" \
  sim_main.cpp \
  src/renode_bus.cpp \
  src/renode_dpi.cpp \
  src/buses/bus.cpp \
  src/IntegrationLibrary/src/communication/socket_channel.cpp \
  src/IntegrationLibrary/libs/socket-cpp/Socket/TCPClient.cpp \
  src/IntegrationLibrary/libs/socket-cpp/Socket/Socket.cpp \
  obj_dir/Vagriguard_top.cpp \
  obj_dir/Vagriguard_top___024root__0.cpp \
  obj_dir/Vagriguard_top___024root__Slow.cpp \
  obj_dir/Vagriguard_top__Syms__Slow.cpp \
  obj_dir/Vagriguard_top___024root__0__Slow.cpp \
  "$VERILATOR_DIR/verilated.cpp" \
  "$VERILATOR_DIR/verilated_vcd_c.cpp" \
  $VERILATOR_THREADS \
  -o Agriguard.exe \
  -lws2_32 \
  -Wno-unknown-pragmas \
  2>&1 | tee build.log

if [ ${PIPESTATUS[0]} -eq 0 ]; then
    echo ""
    echo "=============================================="
    echo "  BUILD SUCCESSFUL! ✓"
    echo "=============================================="
    ls -la Agriguard.exe
    echo ""
    echo "Run standalone test:"
    echo "  ./Agriguard.exe"
    echo ""
    echo "Run with Renode:"
    echo "  ./Agriguard.exe <receiver_port> <sender_port>"
else
    echo ""
    echo "=============================================="
    echo "  LINKING FAILED - Undefined references:"
    echo "=============================================="
    grep "undefined reference" build.log | head -10
fi
