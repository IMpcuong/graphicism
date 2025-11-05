#!/bin/bash

clang++ --std=c++20 -I./metal \
  -fsanitize=address \
  -framework Foundation \
  -framework Metal -framework MetalKit \
  -framework QuartzCore -framework AppKit \
  -o main main.cpp && \
  ./main
