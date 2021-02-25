HAL definition and implementation for "sysInfo".
Each platform implementation will utilize the "public" header
to implement the necessary functions specific to the
hardware.  

There is a "stub" area that is used for normal compilation,
which produces an empty implementation utilized during build
so that the platform dependent implementation can be installed
and exercised during runtime.

