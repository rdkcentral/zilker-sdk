##
## define the structure of handling a request and returning for a reply
## meant for the 'service side' of things
##
	    case ${ipc_code_prefix}${message_code}:
	    {
${handle_vars}
            icLogDebug("${service}", "processing request ${message_code}");
${decode_input}

            // call handle function stub
${call_api_function}

${encode_output}
			break;
		}
