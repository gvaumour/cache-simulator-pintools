#!/bin/bash

myvariable=$USER;
PIN="";

if [[ $USER == "gvaumour" ]]; then
	PIN=/home/gvaumour/Dev/apps/pin/pin-3.2-81205-gcc-linux/pin
else
	PIN=/home/gregory/apps/pin/pin-3.2-81205-gcc-linux/pin
fi


$PIN -t ./obj-intel64/roeval.so -- ./test_apps/applis/deriche/exec_plugin 

