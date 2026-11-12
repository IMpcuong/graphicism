#!/bin/bash

set -euo pipefail

clang++ --std=c++20 -I./metal \
  -fsanitize=address \
  -c main.cpp -o main.o

clang++ --std=c++20 -I./metal \
  -fsanitize=address \
  -fobjc-arc \
  -c mouse_click_bridge.mm -o mouse_click_bridge.o

clang++ --std=c++20 -I./metal \
  -fsanitize=address \
  -framework Foundation \
  -framework Metal -framework MetalKit \
  -framework QuartzCore -framework AppKit \
  main.o mouse_click_bridge.o \
  -o main

./main
