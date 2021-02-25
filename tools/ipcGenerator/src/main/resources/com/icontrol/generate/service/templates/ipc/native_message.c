##
## define the structure of sending a request and waiting for a reply
## meant for the 'client side' of things
##
{
#ifdef ${c_config_flag}
	IPCCode rc = IPC_SUCCESS;
	IPCMessage	*ipcInParm = createIPCMessage();
	IPCMessage  *ipcOutParm = createIPCMessage();

${encode_input}

	// send request
	//
	rc = sendServiceRequest${isTimeout}(${server_port_name}_IPC_PORT_NUM, ipcInParm, ipcOutParm${readReqTimeoutSecs});
	if (rc != IPC_SUCCESS)
	{
		// request failure
		//
		freeIPCMessage(ipcInParm);
		freeIPCMessage(ipcOutParm);
		return rc;
	}

${decode_output}

	// mem cleanup
	//
	freeIPCMessage(ipcInParm);
	freeIPCMessage(ipcOutParm);

	return rc;
#else	// defined - ${c_config_flag}
	return IPC_SERVICE_DISABLED;
#endif
}
