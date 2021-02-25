
This directory contains the implementation of the XHFW object on the CCSP bus.

---------------------------------------------------------------------------------
|                                                                               |
|                       Steps to Run the XHFW Component                         |
|                                                                               |
---------------------------------------------------------------------------------

1) Push the XHFW component binaries and config XML to the device:
=================================================================
        # cd /tmp
        # rm -f xhRdkIntegrationService
        # rm -f XHFW.xml
        # tftp -gr xhRdkIntegrationService <tftp server ipaddress>
        # tftp -gr XHFW.xml <tftp server ipaddress>
        # chmod -R 777 xhRdkIntegrationService
        # chmod -R 777 XHFW.xml

2) Start the XHFW component process:
===================================
        # ./xhRdkIntegrationService -m XHFW.xml

3) Check the status of the process:
==================================
        # ps | grep xhRdkIntegrationService

Dmcli Commands:
==============

  Check using dbus Path
  =====================
  dmcli eRT getv com.cisco.spvtg.ccsp.xhfw.

  To get List of Paramters:
  ========================
  dmcli eRT getv Device.DeviceInfo.X_RDKCENTRAL-COM_XHFW.

  Get & Set a Parameter:
  =====================
  dmcli eRT getv Device.DeviceInfo.X_RDKCENTRAL-COM_XHFW.AWSIoTEnabled
  dmcli eRT setv Device.DeviceInfo.X_RDKCENTRAL-COM_XHFW.AWSIoTEnabled bool true

  dmcli eRT getv Device.DeviceInfo.X_RDKCENTRAL-COM_XHFW.Status

  dmcli eRT setv Device.DeviceInfo.X_RDKCENTRAL-COM_XHFW.SAT string "Valid SAT string"
