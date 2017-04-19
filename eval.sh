#!/bin/bash
PIN=/home/gvaumour/Dev/apps/pin-3.2-81205-gcc-linux/pin

$PIN -t ./obj-intel64/roeval.so -- ./test/helloWorld/test
