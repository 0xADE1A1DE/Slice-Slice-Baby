# Intel LLC Slice Mapping Retrieval Tool

## Introduction

We describe a tool developed for automatically retrieving the last-level cache slice hashing function used in Intel processors. This function is not publicly disclosed nor described by Intel, negatively impacting the development of processor cache side-channel research. 

To develop this tool, we take several liberties with the systems it is run on, mainly by requiring high privilege levels to expose low level processor performance counter interfaces using our `perfcounters` library. 

As a brief overview, the code works by uses pairs of physical addresses which differ on one bit, which we define *adjacent addresses*. These addresses tell us information about the slice mapping for their differing bit *k*. Thus, the slice mapping can be recovered for every addressable bit of memory. 

This is followed by further intricacies which depend on whether the function is linear (2^n slices), or non-linear (non-2^n slices).

This tool currently only works on Intel Core processors. See `output/` folder for retrieved functions (those labelled with `xeon_`... are adapted from work by [John D. McCalpin](https://repositories.lib.utexas.edu/items/78ed399f-0e5e-41fe-96e1-c12a5acf74d7))

Note: The Intel i9-10900K only allows for uncore performance monitoring in 8/10 of its slices. This tool would need to be augmented with a timing approach to retrieve the function and distinguish all slices (which we have done manually in the past).

## Requirements

This tool requires installation of our custom performance counter interface, `perfcounters` available [here](https://github.com/BMorgan1296/perfcounters) and `../memory-bypass`.

## Install

1. Install `perfcounters`	as a submodule:

`git submodule update --init`

Refer to installation steps inside that repo prior to running the following.

2. Install the custom memory bypass kernel module in `../memory-bypass`. Once built:

`sudo insmod memkit.ko`

*Note: you may need to `rmmod` the module before running*.

3. Then build this codebase:

`sudo ./build.sh`

## Usage

To run the LLC slice function retrieval, use the `run.sh` script. You can optionally `--save` the output to a file in the `./output` directory with timestamp.

`sudo ./run.sh [--save]`

## How do I use the retrieved function?
See `example_hash_function_usage.c` to observe code samples utilising the returned information from this tool, calculating arbitrary address slice values.

We show how to use the two main formats provided to calculate the XOR-reduction using either an xor map or group of masks. Following this is code to determine the slice index of addresses on a 6-core machine, utilising the XOR-reduction stage as well as master sequence.

## ToDo
* Xeon processors (requires modification to `perfcounters` interface).
