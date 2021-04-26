Overview
===================================

Root directory of native 3rdParty dependencies.  As each are written in C, they need to be configured 
for the particular dev/host environment each time they are required.  

To make setup easier (and reduce the opportunity to alter the original code), each project contains
a CMakeLists.txt file so that the code can be downloaded, configured, and built via CMake (where
needed as some environments provide these libraries - such as RDK).



