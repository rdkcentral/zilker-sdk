# Building

The instructions below are broken up by platform.  Many of the concepts are similar across
different Operating Systems, but will have some variations.

## General Concepts and Information

Similar to other software, this has a variety of ways to build.  The preferred approach is
to utilize `recipe.sh` scripts under the corresponding `buildTools/<platform>` subdirectory.  
Each supported platform will contain a `recipie.sh` script that is specific to that Operating System.  
For example: `buildTools/linux/recipe.sh`

### Setup Build Environment

Each platform you build from requires a common set of utilities, however, each platform
may have additional requirements:
- `git` -- _for code extraction_
- `cmake` (3.10 or higher), `make`, `autoconf`, `texinfo` -- _for building source_
- `jdk 8`, `gradle 4.8.1` -- _for ipcGenerator, ZITH, mocked Zigbee_
  - **NOTE** _Newer versions of Java/Gradle may work, but these are the tested versions_

### 3rd-Party Dependencies

Many of the platforms we build for will not supply needed 3rd-Party dependencies and
must build them using the appropriate toolchain.  3rd-Party dependencies will be built 
as part of each `recipe.sh` script execution.  See usage of the platform-specific script 
to explicitly build these dependencies.

### Code Generation Dependency

Early in the build process, a call is made to generate code for events and 
inner-process-communication objects.  The code generation utility is internal and located
in "`tools/ipcGenerator`".  It will build/execute automatically, but can be built
manually via:

~~~
# build ipcGenerator
cd <top-of-tree>
gradle :ipcGenerator:build
~~~


## Linux:

Any flavor of Linux should work, however, all existing development was performed on 
Ubuntu 18.04 and 20.04.

In addition to the common utilities, Linux requires the following additional tools:
- `m4`, `libtool`, `libtool-bin`, `curl`, `lib32z1`, `gcc-multilib`, `doxygen`

~~~~
# install all Linux build dependencies 
sudo apt-get install git cmake autoconf texinfo openjdk-8-jdk openjdk-8-jre \
  m4 libtool libtool-bin curl pkg-config lib32z1 gcc-multilib doxygen
~~~~

Once the prerequisite tools are installed, you will be ready to perform a Linux build.  
Unlike other platforms, the automated unit tests will execute as part of the build.

For starters, get the usage by running the script with zero arguments:

~~~~
cd <top-of-tree>
./buildTools/linux/recipe.sh
linux_recipe:
  options:
  -h         : show this help
  -t         : build 3rd-party
  -m         : build mirror and includes
  -d         : produce doxygen docs (build/docs)
  -D         : build DEBUG
  -R         : build RELEASE
  -v         : verbose build
  -z         : build with zith tests (work in progress)
~~~~

The common option is `-m` to build the **mirror** output.  
Running the `buildTools/linux/recipe.sh -m` script will:

1. Leverage `cmake` to generate `Makefiles`
2. Compile 3rdParty if necessary
3. Compile the code into the `build/linux/mirror` output directory.
4. Build/execute unit tests
5. Provide headers into the `build/linux/mirror/include` directory
6. Optionally produce **doxygen** html files

### Building Tests

Tests are broken into two categories:
1. Unit Tests
2. Manual Tests

#### Unit Tests
The *Unit Tests* are items that are built, executed, and validated at build-time.
Due to the "cross-compiling" nature of our environment, these are only ran in Linux.

#### Manual Tests
The *Manual Tests* are interactive in nature and really meant for development use.
All of these are assumed to be for the native environment and cannot be executed stand-alone.
One example is communicating with a known camera.  These can be built/ran via:

~~~~
# after building with recipe.sh
cd <top-of-tree>/build/linux
make all install manualTest
~~~~


## Raspberry Pi 3 B/B+ (cross-compile):

Must be build on a Linux host, and requires the **raspberry-tools** be installed locally.
See [https://github.com/raspberrypi/tools](https://github.com/raspberrypi/tools) for details.

After downloading the raspberry-tools locally, export the `PI_TOOLS` environment variable
to define the location:

~~~
export PI_TOOLS=/opt/toolchains/raspberry-tools
~~~

Performing the build is similar to **linux**, but uses a different recipe script `./buildTools/pi/recipe.sh`

After build is complete, follow the [Step-by-Step Guide](GUIDE.md) for installing onto a Rapsberry Pi and 
executing with simulated Zigbee devices.

## MacOS:

**NOTE: This is a work in progress and not fully supported at this time**

In addition to the common utilities, MacOS requires the following additional tools:
- `wget`, `gnu-tar`, `libtool` -- _used as glibtool for some 3rdParty items_
- `Xcode` -- _for git, gcc, make, libtool_
- `homebrew` -- _to install missing packages_
- `docker` -- _**optional** can be used for building as linux_

After installing Xcode and homebrew, the following can be executed to obtain the remaining
MacOS prerequisite tools:

~~~~
# install all MacOS build dependencies
brew install pkgconfig cmake wget automake autoconf texinfo libtool coreutils gpg gnu-tar 
~~~~


