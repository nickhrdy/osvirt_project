# Adding Memory Allocators to Our Basic Kernel

We have added 4 types of allocators to the kernel from previous in-class assingnments:

* Naive pageframe allocator
* Buddy System
* Slob Allocator
* User-space allocator

Please boot the images found in this repo using virtual box. Each image provides the output of a small evaluation test for one of the above allocators (excluding the naive allocator).

# Build Instructions
Images can be built with `make.sh`. This files assumes that `fwimage` and the include folder from class assignments are up one folder.