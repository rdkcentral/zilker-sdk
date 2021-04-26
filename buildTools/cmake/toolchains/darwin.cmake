#-----------------------------------------------------------------------------
#
# define a small toolchain for Darwin (set flags)
#
#-----------------------------------------------------------------------------

set(CROSS_OUTPUT $ENV{CROSS_OUTPUT})

# host settings
SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_VERSION 1)

# setup flags based on toolchain location
set(CMAKE_C_FLAGS "-DBUILDENV_mac")
SET(CMAKE_EXE_LINKER_FLAGS "-L${CROSS_OUTPUT}/lib -L/usr/local/lib -L${CROSS_OUTPUT}/local/lib")
SET(CMAKE_SHARED_LINKER_FLAGS "-L${CROSS_OUTPUT}/lib -L/usr/local/lib -L${CROSS_OUTPUT}/local/lib")
SET(CMAKE_MODULE_LINKER_FLAGS "-L${CROSS_OUTPUT}/lib -L/usr/local/lib -L${CROSS_OUTPUT}/local/lib")
