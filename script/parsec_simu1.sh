#!/bin/bash

my_dir="/home/gvaumour/Dev/parsec-3.0/pkgs/"

. ./parsec_config.rc

my_lib="/home/gvaumour/Dev/apps/pin/cache-simulator-pintools/obj-intel64/roeval_release1.so"
pin_root="/home/gvaumour/Dev/apps/pin/pin-3.2-81205-gcc-linux/pin"
pin_flags="-follow-execv -t "$my_lib


output_dir="/home/gvaumour/Dev/apps/pin/cache-simulator-pintools/output/dynamicSaturation/"
output_files="results.out config.ini predictor.out DynamicPredictor.out"

cd ../
make release1

a=0;
for i in "${benchs[@]}"
do

	cd $output_dir
	if [ ! -d $i ]; then	
		 mkdir $i;
	fi 

	cd ${directories[$a]}
	echo "Running "$i
	echo "[---------- Beginning of output ----------]"

	$pin_root $pin_flags -- ${cmd_medium[$a]}
	echo "[---------- End of output ----------]"

	mv $output_files $output_dir/$i/

	((a++));
done 

