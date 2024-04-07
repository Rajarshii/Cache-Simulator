# Cache-Simulator
A simple trace-based cache Simulator - 

The project started from the inspiration of CoffeBeforeArch's post on the same. The code relies on some of the ideas from the blog. 
One objective was to ensure that the implementation facilitates future modifications to enhance the robustness of the simulator.

The following list details some of the current features and planned features:

1. Currently supports a single level of cache(L1) - higher cache level support in progress
2. Only supports LRU replacement policy - more policies to be added
3. Constant miss penalty (L2 or memory latency agnostic) and constant write-back latency - queuing model to be supported later
4. Multi-core(SMP) and Coherency support to be added - need to generate/ find traces for this

More items:
1. Possible optimizations and newer C++ standards usage?
2. Possible parallelization using OpenMP?

### Sample Compilation Command
g++ cache_simulator.cpp --std=c++2a -o cache_sim -O3

### Sample Run Command
./cache_simulator/cache_sim <trace_file_path> <cache_size_in_bytes> <associativity> <block_size> <writeback_penalty> <miss_penalty>

For example:
./cache_simulator/cache_sim cache_traces/traces/mcf.trace 65536 8 32 0 42

### Trace Details
The traces used and the trace format is the same as the one available from the following [website](https://occs.oberlin.edu/~ctaylor/classes/210SP13/cache.html)
Normally, the trace looks like the following:
# <address> <instructions> 
