import os
import subprocess

def parse_arguments():
    from argparse import ArgumentParser

    parser = ArgumentParser('Parsec Simulation for the cache simulation')
    option = parser.add_argument

    option("--output_dir", action="store_true",default="./output/classification/",
           help="Output Directory")

    option("--suite", action="store_true", default="parsec",
           help="Suite to simulate")

    option("--lib", action="store_true", default="roeval",
           help="Suite to simulate")

    return parser.parse_known_args()

args, dummy = parse_arguments()
#print(args);


pin_root = "/home/gvaumour/Dev/apps/pin-3.2-81205-gcc-linux/pin"
parsec_bin = "/home/gvaumour/Dev/parsec-3.0/bin/parsecmgmt"
my_lib = "/home/gvaumour/Dev/apps/cache-simulator-pintools/obj-intel64/roeval_release.so"

suite = args.suite
output_dir = args.output_dir;

		

#./parsecmgmt -a run -p parsec.ferret -n 4 -i simlarge

parsec = [ 'blackscholes', 'bodytrack', 'facesim', 'ferret', 'fluidanimate', 'freqmine', 'swaptions', 'vips', 'x264', 'canneal', 'dedup', 'streamcluster'];
splash2x = [ 'barnes' , 'fmm', 'radiosity', 'raytrace', 'volrend', 'cholesky', 'fft', 'radix', 'ocean_cp', 'ocean_ncp', 'water_nsquared', 'water_spatial', 'lu_cb', 'lu_ncb']

if suite == "parsec":
	benchmarks = parsec;
else:
	benchmarks = splash2x;

output_dir = os.path.abspath(output_dir);
os.chdir(output_dir)

if not os.path.exists(suite):
	os.makedirs(suite);
os.chdir(suite);

full_path = os.path.join(output_dir, suite);

for bench in benchmarks:

	if not os.path.exists(bench):
		os.makedirs(bench);
	os.chdir(bench);

	cmd= pin_root + " -follow-execv -t " + my_lib + " -- " + parsec_bin + " -a run -p " + suite + "." + bench + " -i simlarge -c gcc-hooks -n 1"
	print(cmd)
	subprocess.call(cmd.split());
	os.chdir(full_path);
	
