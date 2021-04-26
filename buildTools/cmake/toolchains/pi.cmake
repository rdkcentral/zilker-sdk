#-----------------------------------------------------------------------------
#
# define the toolchain for cross-compiling Raspberry-Pi
#
#-----------------------------------------------------------------------------

# look for an environment variable to tell us where the Raspberry-Pi Tools were installed
set(PI_TOOLS_ROOT $ENV{PI_TOOLS})
if (NOT PI_TOOLS_ROOT)
    # no environment var, bail
    message(FATAL "Missing environment variable PI_TOOLS")
endif()
set(CROSS_OUTPUT $ENV{CROSS_OUTPUT})

# host settings
SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_VERSION 1)

# define the Pi cross-compiler
set(PI_TOOLS_BASE ${PI_TOOLS_ROOT}/arm-bcm2708/arm-rpi-4.9.3-linux-gnueabihf)
SET(CMAKE_C_COMPILER ${PI_TOOLS_BASE}/bin/arm-linux-gnueabihf-gcc)
SET(CMAKE_CXX_COMPILER ${PI_TOOLS_BASE}/bin/arm-linux-gnueabihf-g++)

# set other variables that will end up in the build/3rdParty/pi/toolchain.sh file
set(TC_CROSS_HOST "arm-linux-gnueabihf")
set(TC_CROSS_HOST2 "linux-generic32")
set(TC_SKIP_CMOCKA "true")

# setup flags based on toolchain location
SET(SYSROOT_PATH "${PI_TOOLS_BASE}/arm-linux-gnueabihf/sysroot")
set(CMAKE_C_FLAGS "-DBUILDENV_ARM -DBUILDENV_pi --sysroot=${SYSROOT_PATH} -O2 -fPIC -std=gnu99")
SET(CMAKE_EXE_LINKER_FLAGS "--sysroot=${SYSROOT_PATH} -L${CROSS_OUTPUT}/lib -Wl,-rpath,${CROSS_OUTPUT}/lib")
SET(CMAKE_SHARED_LINKER_FLAGS "--sysroot=${SYSROOT_PATH} -L${CROSS_OUTPUT}/lib -Wl,-rpath,${CROSS_OUTPUT}/lib")
SET(CMAKE_MODULE_LINKER_FLAGS "--sysroot=${SYSROOT_PATH} -L${CROSS_OUTPUT}/lib -Wl,-rpath,${CROSS_OUTPUT}/lib")

# disable searching in the local-host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# search for libraries and headers ONLY in the pi directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)