# Watchdog Service Overview

The *watchdog service* provides functionality similar to `init.rc` in that it
will read a configuration file, launch processes, and react if any of  those 
processes die.  Each entry in the configuration file will define the policies 
for startup and death.

On systems that support a "hardware watchdog", this will also interact with the
hardware to "pet the dog".  This provides a safety net that will reboot the system 
if it becomes unresponsive.



