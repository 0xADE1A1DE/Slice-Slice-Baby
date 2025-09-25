# Slice+Slice Baby: Generating Last-Level Cache Eviction Sets in the Blink of an Eye

The code in this repository accelerates state-of-the-art cache side-channel approaches such as Prime+Probe to be *slice-aware*.

The Intel LLC is split into several distinct *slices*, each a separate cache in their own right. From the perspective of an attacker, they must generate eviction sets for each slice to successfully contend with victim memory, however, the sliced design of the LLC hinders this process as it distributes memory (roughly) evenly across each slice. This makes it harder to find addresses, and therefore eviction sets, which all belongs to the same slice.

We incorporate a slice-detection mechanism using microarchitectural weird gates to measure and determine which slice a memory address maps, and use this to partition the memory into bins by slice. Please refer to the paper published on [arxiv](https://arxiv.org/abs/2504.11208) or IEEE (Accepted to the 46th IEEE Symposium on Security and Privacy, IEEE SP 2025) for further details. Otherwise, feel free to get in touch via email!

## Table of Contents

- [Project Structure](#project-structure)
- [Dependencies](#dependencies)
- [Install](#install)
- [Running](#running)
- [Adapting Configs for New Processors](#adapting-configs-for-new-processors)
- [Running Specific Experiments](#running-specific-experiments)
- [License](#license)
- [Authors](#authors)
- [Acknowledgements](#acknowledgements)

## Project Structure

The project directory contains an `include` directory containing header files organised into modules such as `evsets`, `slice_partitioning`, and `util`, along with corresponding source files in `src`, executables sources for various experiments in `tests`, post-processing of generated experiment data in `experiments` and external libraries in `lib`.

The `auxiliary` folder contains two tools, the first our memory bypass kernel module in, `auxiliary/memory-bypass`, required by our slice function recovery tool in `auxiliary/slice-recovery`.

## Dependencies

The only dependency for the top-level codebase is [0xADE1A1DE/AssemblyLine](https://github.com/0xADE1A1DE/AssemblyLine) in the `lib/AssemblyLine` directory as well as a standard C build system.
Our `install.sh` script takes care of its install, please see below.

## Install

To install the project, follow these steps:

1. Navigate to the `scripts` directory:
   ```bash
   cd scripts
   ```

2. Run the `install.sh` script:
   ```bash
   ./install.sh
   ```

   This script performs the following actions:

   - Initialises and updates Git submodules required for the project.
   - Installs essential build tools and dependencies using `apt`.
   - Builds and installs the `AssemblyLine` library:
     ```bash
     pushd ../lib/AssemblyLine
     ./autogen.sh
     ./configure
     make -j
     sudo make install
     popd

     sudo ldconfig
     ```

## Running
This codebase was built and experimented with using Ubuntu 24.04.

To run locally on a machine, ensure there is a certain config available and do:
```bash
cd scripts/
./run.sh $CONFIG_PATH
```
E.g.
```bash
./run.sh ../config/config_12900k.h
```

This will run `tests/l3_fs.c` which will generate LLC eviction sets using our slice-aware technique.

The script retrieves cache information for the CPU:
- Retrieves and sets variables for CPU cache levels (L1, L2, L3) sizes and associativity using `getconf`.
- Although a little clunky, this was a conscious choice to make sure these values were hardcoded at compile time to simplify the management of certain aspects of the eviction set code.

Finally, it builds the project with the retrieved CPU cache information.

## Adapting Configs for New Processors

- Copy the closest existing config in `config/` (e.g. `config/config_6700k.h`) and rename it for your target.
- Update the LLC eviction thresholds (`LLC_BOUND_LOW` and `LLC_BOUND_HIGH`) so they match the eviction behaviour on the new processor; compile and run `tests/set_addr_state_tests.c` to validate the thresholds.
- To simplify the calibration run, temporarily edit `scripts/run.sh` so it executes `../build/set_addr_state_tests` instead of `../build/l3_fs` and observe the three-column eviction counts.
- When the thresholds look stable,  restore `run.sh` to run whichever `tests/` target you wish, i.e. `./run.sh ../config/config_7700k.h`

## Running Specific Experiments

To run a specific experiment, please use the following lookup:

| Experiment in Paper      | Run Script (`scripts/`) | Post-Processing (`experiments/`) | Notes |
| ---------        | ---------  | --------- | --------- |
| Figure 3         | `run_experiment_llc_slice_timings.sh`                        | `llc_slice_timings/process.py`                        |                                              |
| Figure 4         | `run_experiment_llc_rdtscp_NOT_gate_quiet_predictions.sh`    | `llc_rdtscp_NOT_gate_quiet_predictions/process.py`    |                                              |
| Figure 5         | `run_experiment_llc_NOT_gate_probabilities.sh`               | `llc_NOT_gate_probabilities/process.py`               |                                              |
| Figure 6         | `run_experiment_llc_rdtscp_NOT_gate_busy_predictions.sh`     | `llc_rdtscp_NOT_gate_busy_predictions/process.py`     |                                              |
| Figure 8         | `run_experiment_llc_COMPARATOR_gate_timings.sh`              | `llc_COMPARATOR_gate_timings/process.py`              | Use `config/config_6700k_llc_timings.h`      |
| Figure 9         | `run_experiment_llc_COMPARATOR_gate_predictions.sh`          | `llc_COMPARATOR_gate_predictions/process.py`          | Use `config/config_6700k_llc_timings.h`      |
| Table 2          | `run_experiment_per_core_llc_COMPARATOR_gate_predictions.sh` | `per_core_llc_COMPARATOR_gate_predictions/process.py` |                                              |
| Table 3          | `run_experiment_per_core_llc_*_predictions.sh`               | `compile_table_3.py`                                  |                                              |
| Table 5          | `run_experiment_llc_evsets_*.sh`                             | `compile_table_5.py`                                  |                                              |

## License

Please see LICENSE file in the top directory.

The code in the `lib/AssemblyLine` submodule is licensed under the Apache License 2.0. See [AssemblyLine/LICENSE](https://github.com/0xADE1A1DE/AssemblyLine/blob/main/LICENSE) for more details.

## Authors
* [Bradley Morgan](https://about.bradm.io) (The University of Adelaide, Defence Science and Technology Group)
* Gal Horowitz (Tel-Aviv University)
* Sioli O’Connell (The University of Adelaide)
* [Stephan van Schaik](https://codentium.com/about/) (University of Michigan)
* [Chitchanok Chuengsatiansup](https://chitchanok.org) (The University of Klagenfurt)
* [Daniel Genkin](https://www.cc.gatech.edu/~genkin/) (Georgia Tech)
* [Olaf Maennel](https://maennel.net) (The University of Adelaide)
* Paul Montague (Defence Science and Technology Group)
* [Eyal Ronen](https://eyalro.net/) (Tel-Aviv University)
* [Yuval Yarom](https://yuval.yarom.org) (Ruhr University Bochum)

## Acknowledgements
This work was supported by:
* The Air Force Office of Scientific Research (AFOSR) under award number FA9550-24-1-0079
* The Alfred P. Sloan Research Fellowship
* An ARC Discovery Project number DP210102670
* The Defense Advanced Research Projects Agency (DARPA) under contract numbers W912CG-23-C-0022
* Defence Science and Technology Group (DSTG), Australia under Agreement No. 11965
* The Deutsche Forschungsgemeinschaft (DFG German Research Foundation) under Germany’s Excellence Strategy - EXC 2092 CASA - 390781972
* ISF grant no. 1807/23; Len Blavatnik and the Blavatnik Family Foundation
* Stellar Development Foundation
* and gifts from Cisco and Qualcomm.

The views and conclusions contained in this document are those of the authors and should not be interpreted as representing the official policies, either expressed or implied, of the U.S. Government.
