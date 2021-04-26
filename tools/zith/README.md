# ZITH

## Overview
ZITH (Zilker Integration Test Harness) is a framework for running integration style tests against the Zilker codebase.

### ZITH Shell
For development purposes the components in the ZITH framework can be controlled via an interactive shell.  The
MockZigbeeCore can be launched within the shell.  There is a help command to see 
the list of available commands.  The ZITH shell is launched via the included shell script:
```
$ZILKER_SDK_TOP/tools/zith/zithShell.sh
```

Once the shell and `MockZigbeeCore` are running, the zilker codebase can execute and
interact with the mocked environment.  This allows interaction with simulated zigbee
devices.

**NOTE:** The build depends on generated code, which occurs during the normal zilker build.
This means zilker must be compiled before attempting to run the `zithShell.sh`
