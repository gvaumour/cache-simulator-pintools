#!/bin/bash

myvariable=$USER;
PIN="";

if [[ $USER == "gvaumour" ]]; then
	PIN=/home/gvaumour/Dev/apps/pin/pin-3.2-81205-gcc-linux/pin
else
	PIN=/home/gregory/apps/pin/pin-3.2-81205-gcc-linux/pin
fi

#declare -a arr=("adi" "deriche_opt" "max33" "median33" "jacobi" "mat_multiply");
#declare -a arr=("deriche");
#output_files="results.out config.ini log.out"

cd dump 
#mkdir test
#cd test
#$PIN -t ../obj-intel64/roeval.so -- ../test_apps/parallel/test 
$PIN -t ../obj-intel64/roeval.so -- ../test_apps/applis/deriche/exec_plugin 
/home/gvaumour/Dev/apps/energy_model/energy_model > energy.out

#for i in "${arr[@]}"
#do
#	echo "Benchmark : "$i
#	$PIN -t ./obj-intel64/roeval.so -- ./test/compilerTest/test
#	mv $output_files ./output/applis/reference/$i	
#done

#$PIN -t ./obj-intel64/roeval.so -- ./test/helloWorld/test

