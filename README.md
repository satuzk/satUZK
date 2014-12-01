
# satUZK

Authors: Alexander van der Grinten, Andreas Wotzlaw and Ewald Speckenmeyer

University of Cologne

## How to build

Prerequisites:
- Linux kernel 2.6 or later
- GCC 4.7 or later

The parallel solver uses C++11's std::thread. Make sure your compiler supports this feature.

Run `make` to build the solver.

## How to use

`satUZK-seq` is the basic sequential solver and accepts a SAT instance
in DIMACS CNF format. `satUZK-par` is a parallel version.

Use `./satUZK-seq <parameters> <instance file>` to run the sequential solver
and `./satUZK-par <instance file>` to run the parallel solver.

The following parameters are supported for `satUZK-seq`:
- `-show-model` Prints a model if the instance is satisfiable

