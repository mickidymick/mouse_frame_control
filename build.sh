#!/bin/bash
gcc -o mouse_frame_control.so mouse_frame_control.c $(yed --print-cflags) $(yed --print-ldflags)
