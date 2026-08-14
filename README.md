# q3lite on Raspberry Pi bare metal
A work-in-progress port of **q3lite** running directly on Raspberry Pi hardware without an operating system.

This project combines the Quake III Arena lightweight engine (q3lite) with the bare-metal capabilities of **Raspberry Pi** provided by **RSTA2 Circle** and **circle-stdlib**. The goal is to bring a classic 3D game engine experience to the Raspberry Pi platform while running entirely from bare metal.

## Status

**Feature complete**

The project currently:

* ✅ Boots and runs on Raspberry Pi 2 or 3 hardware
* ✅ Hardware Accelerated OpenGLES1.1
* ✅ Keyboard and mouse working
* ✅ Sound working via HDMI
* ✅ JIT Compiler for QVM (working with caveats)
* ✅ Networking working (only on Pi 3)

The project is now feature complete with first round of performance improvements. Currently I get over 50fps using vertex lighting, or 40fps using lightmaps

## Supported hardware
The project has been tested on:

* Raspberry Pi 2 (32-bit) but does not have wifi, so no networking
* Raspberry Pi 3 (32-bit)

Raspberry Pi 4 and 5 are not supported unfortunately because VCHIQ driver does not support these models

## Architecture
Unlike traditional Raspberry Pi software running under Linux, this project runs directly on the hardware:

Raspberry Pi Hardware
        |
        v
RSTA2 Circle-stdlib
        |
        v
q3lite engine

Circle provides the low-level platform support required for booting, hardware access, graphics, networking, and device management, while q3lite provides the game engine layer.

## Goals
The main goals of this project are:

* Run q3lite as a true bare-metal application
* Explore the feasibility of running a classic 3D engine without an operating system
* Take advantage of Circle's direct hardware access for predictable and stable gameplay

## Getting the source
This project uses Git submodules. Clone the repository and initialise all submodules with:

```git clone --recursive https://github.com/IanCassTwo/q3lite-circle.git```

If you already cloned the repository without --recursive, initialise the submodules afterwards:

```git submodule update --init --recursive```

To update all submodules to their latest configured revisions:

```git submodule update --recursive --remote```

### Prerequisites
You will need:
* A suitable ARM cross-compilation toolchain, details here https://github.com/rsta2/circle/tree/master#build
* make
* A Linux build environment
* This repository cloned with submodules: ```git clone --recursive https://github.com/IanCassTwo/q3lite-circle.git```

## Building
Building is a manual process right now. You have to do the following:-

### Build Circle-stdlib
* change directory to libs/circle-stdlib
* run ```./configure --kernel-max-size=64 -r2``` where the -r parameter specifies Raspberry Pi model number (1, 2, 3, 4, 5, default: 1)
* make
* Once the build is complete, complete the following:-
  * ```cd libs/circle/addon/linux``` and then ```make```
  * ```cd libs/circle/addon/vc4``` and then ```./makeall --nosample```
  * ```cd libs/circle/addon/vc4/interface``` and then ```./makeall```

### Build the Circle Kernel
* change directory to src
* ```make```

### Build your SD card
* Format a card to fat32 and populate it with firmware files according to [this Circle documentation](https://github.com/rsta2/circle/blob/master/boot/README)
* Copy .img file that was generated in the build step above to your SD card
* Create a baseq3 directory using a retail pak0.pk3 and latest point release. You can get the files you need here https://github.com/nrempel/q3-server/tree/master. Note, Quake3 is a commercial game and you should ensure you own the original before using this
* Insert into pi and power on

## Running
The resulting image is copied to a Raspberry Pi boot medium and launched directly by the firmware. No Linux distribution or operating system installation is required.

## Roadmap
TODO list includes:

* [ ] Improved hardware compatibility testing
* [ ] Performance tuning including multi-core

## QVM JIT
The QVM JIT compiler requires dynamic memory execution. Because Circle does not implement POSIX `mmap()` or `mprotect()`, heap memory is marked as Execute-Never (`XN`) by default, preventing execution of dynamically emitted native ARM code.

To enable the JIT compiler, complete the following two steps:

1) **Enable Executable Heap Memory in Circle:** Find pagetable.cpp inside Circle under the circle-stdlib source rree. Change the nAttributes from `ARMV6MMUL1SECTION_NORMAL_XN` to `ARMV6MMUL1SECTION_NORMAL`. Note on Security: Disabling the XN bit globally allows code to be executed directly from RAM heap sections. This will remove hardware-level execution protection against stack/heap buffer overflows. It's up to you to evaluate the risk.
2) **Enable the JIT Build Flag** Add -DCIRCLE_VM_JIT_EXPERIMENTAL to your CFLAGS in your Makefile 

## Background
This project is an experiment in combining modern embedded hardware with classic PC-era game technology. Running q3lite on bare metal provides an interesting challenge: recreating the services normally provided by an operating system while keeping the simplicity and efficiency of direct hardware execution.

## License & Attribution
This project is licensed under the GNU General Public License v3.0 (GPLv3) or later.

### Third-Party Dependencies
- **q3lite**: Licensed under GNU GPLv3 (or later) © cdev-tux / id Software / ioquake3 contributors.
- **circle-stdlib**: Licensed under GNU GPLv3 © Stephan Muehlstrasser and contributors.
- **Circle**: Licensed under GNU GPLv3 © René Stange.
