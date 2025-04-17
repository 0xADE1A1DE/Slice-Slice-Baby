# Prune-PlumTree---Finding-Eviction-Sets-at-Scale
That repository contains the source code of the paper "Prune+PlumTree - Finding Eviction Sets at Scale"
Instructions for Compilation:
1. Begin by opening the main.h file and making modifications to the values of W, SetsLLC, and THRESHOLD according to your specific machine. 
   Here, W represents the associativity of the LLC, SetsLLC refers to the number of cache sets in the LLC, and THRESHOLD represents the threshold differentiating between cache hits and misses.
2. Open the terminal at the same location as the files.
3. Execute the "make" command.
4. If your intention is to independently identify all eviction sets in the LLC , you need to disable all prefetchers. To achieve this, enter the following command in the terminal: "sudo wrmsr 0x1a4 -a 0xf".  
   (The Proof-of-Concept mode is specifically designed and optimized for the i7-9700K Intel CPU, which means it may not perform as well on other CPUs, potentially resulting in a decrease in performance.)
   If you only wish to find all page heads, proceed to step 5.
5. Run the program by executing the command "./main".
