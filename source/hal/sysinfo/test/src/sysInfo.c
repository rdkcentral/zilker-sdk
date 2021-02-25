/*
 * Copyright 2021 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/****************************************************************************
*
* @file		sysInfo.c
* @brief	Executable application to be called by scripts and from JAVA.
*         Called by init.boot.sh and UpgradeService.java. 
*
****************************************************************************/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sysinfo/sysinfoHAL.h>
#include "sysInfo.h"

/**
 * @brief: This executable application allows for Runtime calls from Java to the native sysinfo HAL.
 * @param argc : Number of arguments provided
 * @param argv : a list of the argument strings
 * @brief : Argument options, only call one:
 *    -m : get the MAC address
 *    -v : get the hardware version
 *    -s : get the serial number
 *    -G : get the device memory lockdown value
 *    -S : set the device for memory lockdown      
 * @return 0 for success, error code otherwise
 */
 int main(int argc, char *argv[])
{
  int   ret, opt;

  while ((opt = getopt(argc, argv, "mvsGS")) != -1)
  {
    switch(opt)
    {
      case 'm':
          // get MAC address
          ret = sysinfo_mac_address();
          break;
      case 'v':
          // get hardware version
          ret = sysinfo_hw_version();
          break;
      case 's':
           // get serial number
          ret = sysinfo_serial_number();
          break;
      case 'G':
          // get the device memory lock value
          ret = sysinfo_get_dev_mem_lock();
          break;
      case 'S':
          // set the device memory lock
          ret = sysinfo_set_dev_mem_lock();
          break;
      default:
          printf("sysinfo non-option argument.\n");
          ret = -1;
          break;
    }
  }
  return ret;
}


int sysinfo_mac_address(void)
{
  uint8_t macAddress[6];
  int ret, i;
  
  // Get the MAC Address
  ret = hal_sysinfo_get_macaddr(&macAddress[0]);
  if(ret==0)
  {
    // print the MAC address   
  	for(i = 0;i < 5; i++)
  	{
  		printf("%.2x:",macAddress[i]);
  	}
  	printf("%.2x",macAddress[i]);
  }
  else
  {
    // error case
    printf("00:00:00:00:00:00");
  }
  return ret;
}

int sysinfo_hw_version(void)
{
	// up-to 10 character h/w version is allowed
	char hwVersion[11] = {0};
	int ret;

	// Get the hardware version
	ret = hal_sysinfo_get_hwver(hwVersion, 10);
	if(ret==0)
	{
		printf("%s", hwVersion);
	}
	else
	{
		printf("Error getting hw version = %d\n", ret);
	}
	return ret;
}

int sysinfo_serial_number(void)
{
  char serialNumber[SERNUM_LENGTH];
  int ret;

  // Get the Serial Number
  memset(serialNumber, '\0', SERNUM_LENGTH);
  ret = hal_sysinfo_get_serialnum(&serialNumber[0], SERNUM_LENGTH-1);
  if(ret==0)
  {
    printf("%s", serialNumber);
  }
    else
  {
    printf("Error-Not-Found");
  }
  return ret;
}

int sysinfo_get_dev_mem_lock(void)
{
  bool bValue;
  
  // Get the NAND boot info
  bValue = hal_sysinfo_get_capability(SYSINFO_DEVICE_MEM_LOCKDOWN);

  if(bValue)
      printf("Device Memory Lockdown = TRUE\n");
  else
      printf("Device Memory Lockdown = FALSE\n"); 
  
  return EXIT_SUCCESS;
}

int sysinfo_set_dev_mem_lock(void)
{
  int ret;

  ret = hal_sysinfo_set_capability(SYSINFO_DEVICE_MEM_LOCKDOWN);
  if(ret == 0)
  {      
    printf("Programming Device Memory Lockdown SUCCEEDED. \n");
  }
  else
  {
    printf("Programming Device Memory Lockdown FAILED!!! \n");
  }
  return ret;
}  
