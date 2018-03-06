#!/bin/bash


. ./parsec_config.rc

my_lib="/home/gvaumour/Dev/apps/pin/cache-simulator-pintools/obj-intel64/roeval.so"
pin_root="/home/gvaumour/Dev/apps/pin/pin-3.2-81205-gcc-linux/pin"
pin_flags="-follow-execv -t "$my_lib


output_dir="/home/gvaumour/Dev/apps/pin/cache-simulator-pintools/output/test/"
output_files="config.ini  *out"


test="x264"

a=0;
for i in "${benchs[@]}"
do
	if [ $i == $test ]; then

		cd $output_dir
	
		cd ${directories[$a]}
		echo "Running "$i
		echo "[---------- Beginning of output ----------]"
		$pin_root $pin_flags -- ${cmd_medium[$a]}
		echo "[---------- End of output ----------]"

		mv $output_files $output_dir/
	
	fi
	

	((a++));
done 

