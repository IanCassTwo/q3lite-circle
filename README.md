# q3lite on Raspberry Pi bare metal

A work-in-progress port of **q3lite** running directly on Raspberry Pi hardware without an operating system.

This project combines the Quake III Arena lightweight engine (q3lite) with the bare-metal capabilities of **Raspberry Pi** provided by **RSTA2 Circle** and **circle-stdlib**. The goal is to bring a classic 3D game engine experience to the Raspberry Pi platform while running entirely from bare metal.

## Status

🚧 **Early work in progress**

The project currently:

* ✅ Builds successfully
* ✅ Boots and runs on Raspberry Pi hardware
* ✅ Produces graphical output
* ❌ Keyboard input not yet implemented
* ❌ Mouse input not yet implemented
* ❌ Sound output not yet implemented

The current focus is bringing up the remaining platform functionality required for a complete playable experience.

## Supported hardware

The project has been tested on:

* Raspberry Pi 2 (32-bit)

It is expected to support other Raspberry Pi models supported by Circle, although hardware-specific testing is still ongoing.

## Architecture

Unlike traditional Raspberry Pi software running under Linux, this project runs directly on the hardware:


Raspberry Pi Hardware
        |
        v
RSTA2 Circle
        |
        v
circle-stdlib
        |
        v
q3lite engine


Circle provides the low-level platform support required for booting, hardware access, graphics, and device management, while q3lite provides the game engine layer.

## Goals

The main goals of this project are:

* Run q3lite as a true bare-metal application
* Explore the feasibility of running a classic 3D engine without an operating system
* Provide a reusable foundation for game engine ports on Raspberry Pi
* Take advantage of Circle's direct hardware access

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
* run 
./configure --kernel-max-size=64 -r2
 where the -r parameter specifies Raspberry Pi model number (1, 2, 3, 4, 5, default: 1)
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

Planned work includes:

* [ ] Keyboard input support
* [ ] Mouse input support
* [ ] Audio support
* [ ] Improved hardware compatibility testing
* [ ] Performance tuning including multi-core
* [ ] Additional platform integration

## Background

This project is an experiment in combining modern embedded hardware with classic PC-era game technology. Running q3lite on bare metal provides an interesting challenge: recreating the services normally provided by an operating system while keeping the simplicity and efficiency of direct hardware execution.

## License

See the individual component licenses:

* q3lite
* RSTA2 Circle
* circle-stdlib

This project may contain code derived from or inspired by these projects and remains subject to their respective licenses.
