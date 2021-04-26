#-----------------------------------------------------------------------------
#
# define a small toolchain for Linux (set flags)
#
#-----------------------------------------------------------------------------

set(CROSS_OUTPUT $ENV{CROSS_OUTPUT})

# host settings
SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_VERSION 1)

# define gcc compiler
SET(CMAKE_C_COMPILER /usr/bin/gcc)
SET(CMAKE_CXX_COMPILER /usr/bin/g++)

# setup flags based on toolchain location
set(CMAKE_C_FLAGS "-DBUILDENV_linux -fPIC -std=gnu99")
SET(CMAKE_EXE_LINKER_FLAGS "-L${CROSS_OUTPUT}/lib -Wl,-rpath,${CROSS_OUTPUT}/lib")
SET(CMAKE_SHARED_LINKER_FLAGS "-L${CROSS_OUTPUT}/lib -Wl,-rpath,${CROSS_OUTPUT}/lib")
SET(CMAKE_MODULE_LINKER_FLAGS "-L${CROSS_OUTPUT}/lib -Wl,-rpath,${CROSS_OUTPUT}/lib")
