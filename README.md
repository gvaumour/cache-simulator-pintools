
Cache Simulator for Hybrid Cache Evaluation 
===========================================

This project is a cache simulator associated with Pintools. It is currently developped at Uppsala University in the UART group for quick evaluation of different strategies for data placement specially with hybrid cache architecture where the cache is divided into several regions with different memory technologies

This framework is made for: 
* Evaluation of predictors for hybrid SRAM/NVM cache architecture
* Evaluation of cache replacement/insertion policies
* Evaluation of prefetching strategies
* Evaluation of dead block prediction strategies 


## Compilation 

Verified with pintool version 3.6, and update the PIN_ROOT variable in the Makefile with your pin location. It can be downloaded [here][1] 

```bash 
cd <path to your cache-simulator-pintools.git clone>
make
```

## Running 

$PIN_ROOT/pin -t ./obj-intel64/roeval.so -- <your app to evaluate>

[1]: https://software.intel.com/en-us/articles/pin-a-binary-instrumentation-tool-downloads
