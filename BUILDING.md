# Building

## Quick Steps
If you're like most developers and don't feel like reading instructions, you can
attempt these steps as a quick-n-dirty "just build it" approach for a workstation target.  
When this fails, please consider reading the rest of this file before asking stupid questions to others:

~~~~
# setup env
cd <wherever-the-code-is>
export ZILKER_SDK_TOP=`pwd`

# build and let script show steps necessary
./buildTools/linux/recipe.sh -m
~~~~

## Setup Build Environment

Each platform you build from (suggestion is to use linux), requires a set of utilities:
- git -- _for code extraction_
- cmake (3.10 or higher), make, autoconf, texinfo -- _for building source_
- jdk 8, gradle 4.8.1 -- _for ipcGenerator, ZITH, mocked Zigbee_
  - **NOTE** _Newer versions of Java/Gradle may work, but these are tested versions_

### Ubuntu (tested on 14.04, 16.04, 18.04, 20.04)

Additional tools for Linux:
- m4, libtool, libtool-bin, curl
- lib32z1, gcc-multilib, doxygen

~~~~
# enable building base code
sudo apt-get install git cmake autoconf texinfo openjdk-8-jdk openjdk-8-jre \
  m4 libtool libtool-bin curl pkg-config lib32z1 gcc-multilib doxygen
~~~~

### macOS:

Additional tools for Linux:
- **NOTE: Not fully supported at this time**
- wget, gnu-tar, libtool -- _used as glibtool for some 3rdParty items_
- Xcode -- _for git, gcc, make, libtool_
- homebrew -- _to install missing packages_
- docker -- _**optional** can be used for building as linux_

~~~~
# enable building base code
brew install pkgconfig cmake wget automake autoconf texinfo libtool coreutils gpg gnu-tar 
~~~~

### Raspberry Pi (cross-compile):

Must be build on a linux host, and requires the raspberry-tools be installed locally.
See [https://github.com/raspberrypi/tools](https://github.com/raspberrypi/tools) for details.

After downloading the raspberry-tools locally, export the `PI_TOOLS` environment variable 
to define the location:

~~~
export PI_TOOLS=/opt/toolchains/raspberry-tools
~~~

Performing the build is similar to **linux**, but uses a different recipe script `./buildTools/pi/recipe.sh`

### Environment Variables
Define the location of the source tree as the environment variable `ZILKER_SDK_TOP`.
It is suggested that this variable is added to your main shell environment (ex: `~/.bashrc`).

## 3rd-Party Dependencies

Many of the platforms we build for will not supply needed 3rd-Party dependencies.
Therefore we must build them using the appropriate toolchain.  If missing, the
3rd-Party dependencies will be built as part of each `recipe.sh` script execution.
See usage of that script to explicitly build these dependencies.

## Code Generation Dependency

Early in the cmake configuration step, a call is made to generate some code.
This depends on the "`ipcGenerator`" utility, which will be automatically built.
If for some reason the code generation utility needs a rebuild, it can be built
manually via:

~~~
# build ipcGenerator
cd $ZILKER_SDK_TOP
gradle :ipcGenerator:build
~~~

## Build Zilker-SDK Codebase

Running the `recipe.sh` will leverage cmake to generate `Makefiles`, apply a 
branding, then compile the code into the `build/linux` output directory.
For starters, get the usage by running the script with zero arguments:

~~~~
# ensure the $ZILKER_SDK_TOP environment variable is set
cd $ZILKER_SDK_TOP
./buildTools/linux/recipe.sh
linux_recipe:
  options:
  -h         : show this help
  -t         : build 3rd-party
  -m         : build mirror
  -D         : build DEBUG
  -R         : build RELEASE
  -v         : verbose build
  -z         : build with zith tests
~~~~

To run the built development code, you'll need to source in the environment (if not done before)
because the development scripts utilize those variables for the path locations:

~~~~
# source environment
. $ZILKER_SDK_TOP/buildTools/setup_env.sh linux

# built code resides in the "mirror" directory
cd $ZILKER_SDK_TOP/build/linux/mirror
./bin/xhStartup.sh
~~~~

## Building Tests

Tests are broken into two categories:
1. Unit Tests
2. Manual Tests

### Unit Tests
The *Unit Tests* are items that can be build, executed, and validated at build-time.
Due to the "cross-compiling" nature of our environment, these should only be ran when the target
matches the machine (i.e. linux).  These are built/ran during the `buildTools/linux/recipe.sh` execution.

### Manual Tests
The *Manual Tests* are interactive in nature and really meant for development use.
All of these are assumed to be for the native environment and cannot be executed stand-alone.
Examples include communicating with a known camera or zigbee device (with dependencies on a ZigBee Radio)
These can be built/ran via:
~~~~
# after building main dependencies via recipe.sh
cd $ZILKER_SDK_TOP/build/linux
make all install manualTest
~~~~

