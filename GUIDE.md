# Guide

This is a Step-by-Step guide to build and run the Zilker SDK and a mocked Zigbee controller on a device.
For this example, we will build on a Linux machine and execute on a Raspberry Pi 3 B device

## Raspberry Pi

### Setup Steps

Refer to [Building](BUILDING.md) for details on setting up the environment and basic building
information for the Raspberry Pi.

1. In the Linux dev environment, build the code and produce an installation package

~~~
ubuntu: > cd <top-of-tree>
ubuntu: > ./build/pi/recipe -i
~~~

2. After build is complete, copy the installation package and script to the 
   Raspberry Pi device (USB, `scp`, etc).

~~~
ubuntu: > scp build/pi/install/* pi@192.168.1.10:/tmp 
~~~

3. In the Raspberry Pi terminal, execute the installation script that was copied.  This will
   install the Zilker runtime binaries/libraries and the ZITH Shell so it can serve as
   a "Mocked Zigbee Controller"

~~~
[pi@my-pi:- $ /tmp/installZilker.sh 

Zilker Installation Script for Raspberry Pi 3 B/B+
Requires the following directories, and may leverage 'sudo' to create them.
 zith   : /home/pi/zith   (requires Java 8)
 zilker : /opt/zilker
        : /opt/etc
 zigbee : /opt/zigbeeFirmware

Ready to continue? [Y/n]
Creating directories...
Extracting files...
Installation Complete!
~~~

4. Now that the installation is complete, use two terminals to interact with the Raspberry Pi:
   - One for the ZITH Shell to operate the **Mocked Zigbee Controller**
   - One to interact with **Zilker**

5. In the **ZITH Shell terminal**, start the shell.  Once running, the state of the 
   Mocked Zigbee Controller will be displayed on the bottom row (right side).
   
~~~
[pi@my-pi:- $ cd $HOME/zith
[pi@my-pi:- $ bin/zith 

zith>
Server: STOPPED | Zigbee: STOPPED
~~~

6. The ZITH shell has a lot of commands, which can be viewed via `help`.  To start the
   Mocked Zigbee Controller, simpy enter the command `start-zigbee`.
   The bottom status-line should now show `Zigbee: RUNNING`  

~~~
zith>zigbee-start
zith>
Server: STOPPED | Zigbee: RUNNING   
~~~

7. In the **Zilker terminal**, start the runtime.  After that step, it is best to source in the
   environment so that commands/libraries are set properly.
   
~~~
# start runtime
[pi@my-pi:- $ /opt/zilker/bin/xhStartup.sh

# source environment
[pi@my-pi:- $ source /tmp/xh_env.sh
~~~

### Creating and Pairing a Mocked Device 

Now that the Zilker runtime and ZITH Shell are running, it is possible to create a mocked 
device and onboard it. 

1. Create/Pair a light device

* In the **ZITH Shell terminal**, create the light device using `light-add`.  Once complete, use
the `list` commend to see details

~~~  
zith>light-add
Light 1 MockDevice created with eui64 d32339a08fa2bdc6
Light 1 added
zith>
zith>list
┌───────┬─────────┬────────────────┬──────────┐
│Name   │Type     │EUI64           │Discovered│
├───────┼─────────┼────────────────┼──────────┤
│Light 1│MockLight│d32339a08fa2bdc6│false     │
└───────┴─────────┴────────────────┴──────────┘

zith>
Server: STOPPED | Zigbee: RUNNING      
~~~   

* In the **Zilker terminal**, discover the newly created device using the interactive command `xhDeviceUtil`.
Within that command, request the discovery of the light with `discoverStart light`.
  **NOTE** like ZITH, at any point you can ask `xhDeviceUtil` for `help` to see the list of available commands.

~~~
[pi@my-pi:- $ xhDeviceUtil
deviceUtil> discoverStart light
Starting discovery of light for 60 seconds
deviceUtil> 
discoveryStarted

device discovered! uuid=d32339a08fa2bdc6, manufacturer=CentraLite, model=4257050-ZHAC, hardwareVersion=3, firmwareVersion=0x15005010

device added! deviceId=d32339a08fa2bdc6, uri=/d32339a08fa2bdc6, deviceClass=light, deviceClassVersion=0

endpointAdded: deviceUuid=d32339a08fa2bdc6, id=1, uri=/d32339a08fa2bdc6/ep/1, profile=light, profileVersion=0

deviceDiscoveryCompleted: uuid=d32339a08fa2bdc6, class=light

discoveryStopped

deviceUtil> 
~~~

* Each terminal can now show details on the light

~~~
zith>list
┌───────┬─────────┬────────────────┬──────────┐
│Name   │Type     │EUI64           │Discovered│
├───────┼─────────┼────────────────┼──────────┤
│Light 1│MockLight│d32339a08fa2bdc6│true      │
└───────┴─────────┴────────────────┴──────────┘

zith>
Server: STOPPED | Zigbee: RUNNING  
~~~

~~~~
deviceUtil> listDevices
d32339a08fa2bdc6: Class: light
	Endpoint 1: Profile: light, Label: (null)
	
deviceUtil> printDevice d32339a08fa2bdc6
d32339a08fa2bdc6: (no label), Class: light
	/d32339a08fa2bdc6/r/communicationFailure = false
	/d32339a08fa2bdc6/r/dateAdded = 1616001395204
	/d32339a08fa2bdc6/r/dateLastContacted = 1616001395204
	/d32339a08fa2bdc6/r/feLqi = 255
	/d32339a08fa2bdc6/r/feRssi = -50
	/d32339a08fa2bdc6/r/firmwareUpdateStatus = upToDate
	/d32339a08fa2bdc6/r/firmwareVersion = 0x15005010
	/d32339a08fa2bdc6/r/hardwareVersion = 3
	/d32339a08fa2bdc6/r/manufacturer = CentraLite
	/d32339a08fa2bdc6/r/model = 4257050-ZHAC
	/d32339a08fa2bdc6/r/neLqi = (null)
	/d32339a08fa2bdc6/r/neRssi = (null)
	/d32339a08fa2bdc6/r/resetToFactoryDefaults = (null)
	Endpoint 1: Profile: light
		/d32339a08fa2bdc6/ep/1/r/currentLevel = 0
		/d32339a08fa2bdc6/ep/1/r/currentPower = 0
		/d32339a08fa2bdc6/ep/1/r/isDimmableMode = true
		/d32339a08fa2bdc6/ep/1/r/isOn = false
		/d32339a08fa2bdc6/ep/1/r/label = (null)
~~~~

* Turn on the light from `xhDeviceUtil`, then see that it's on from the ZITH Shell

~~~
deviceUtil> writeResource /d32339a08fa2bdc6/ep/1/r/isOn true
deviceUtil> 
resourceUpdated: id=isOn, uri=/d32339a08fa2bdc6/ep/1/r/isOn, ownerId=1, ownerClass=light, value=true, type=com.icontrol.boolean, mode=0x7b (rw-del-)
~~~

~~~
zith>light-list 
┌───────┬────────────────┬──────────┬────┐
│Name   │EUI64           │Discovered│On  │
├───────┼────────────────┼──────────┼────┤
│Light 1│d32339a08fa2bdc6│true      │true│
└───────┴────────────────┴──────────┴────┘

zith>
Server: STOPPED | Zigbee: RUNNING     
~~~

* Turn off the light from ZITH Shell, and see that state reflect in `xhDeviceUtil`

~~~
zith>light-turn-off
Light Light 1 is now off
zith>
Server: STOPPED | Zigbee: RUNNING  
~~~

~~~
deviceUtil> 
resourceUpdated: id=isOn, uri=/d32339a08fa2bdc6/ep/1/r/isOn, ownerId=1, ownerClass=light, value=false, type=com.icontrol.boolean, mode=0x7b (rw-del-)

deviceUtil> readResource /d32339a08fa2bdc6/ep/1/r/isOn
false
deviceUtil> 
~~~
