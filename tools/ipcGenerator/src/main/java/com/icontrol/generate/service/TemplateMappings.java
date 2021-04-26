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

package com.icontrol.generate.service;

//-----------
//imports
//-----------

//-----------------------------------------------------------------------------
// Class definition:    TemplateMappings
/**
 *  List of the substitution key/mappings our various templates use
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public interface TemplateMappings
{
    /*---------------------------------------
     *
     * Generic keys used during generation
     *
     *---------------------------------------*/

    // name of the service being processed
    //
    public static final String SERVICE_KEY    = "service";      // name of the service the generation is for (ex: AppMgr)

    // target service port (both IPC & Event)
    //
    public static final String SERVER_PORT_NAME  = "server_port_name";
    public static final String SERVER_PORT_NUM   = "server_port_num";

    /*
     * JSON encode/decode keys used by our primitive types that are ObjectIPC instances (in Java)
     */
    public static final String INT_JSON_KEY    = "i"; //IntegerIPC.MY_KEY;
    public static final String LONG_JSON_KEY   = "l"; //LongIPC.MY_KEY;
    public static final String FLOAT_JSON_KEY  = "f"; //FloatIPC.MY_KEY;
    public static final String DOUBLE_JSON_KEY = "d"; //DoubleIPC.MY_KEY;
    public static final String STRING_JSON_KEY = "s"; //StringIPC.MY_KEY;
    public static final String BOOL_JSON_KEY   = "b"; //BooleanIPC.MY_KEY;
    public static final String DATE_JSON_KEY   = "t"; //DateIPC.MY_KEY;
    public static final String JSON_JSON_KEY   = "j"; //JsonIPC.MY_KEY;


    /*---------------------------------------
     *
     * Common keys used during Java generation
     *
     *---------------------------------------*/

    public static final String JAVA_CLASS           = "java_class";          // ex: [MySrc]
    public static final String JAVA_PKG             = "java_pkg";            // ex: [com.icontrol.api.comm]
    public static final String JAVA_IMPORT_SECTION  = "java_import_sec";     // "import" section
    public static final String JAVA_VAR_SECTION     = "java_var_sec";        // "instance variable" section
    public static final String JAVA_GET_SET_SECTION = "java_get_set_sec";    // getters & setters section
    public static final String JAVA_FREE_FORM       = "java_free_form";      // free-form injected code
    public static final String JAVA_TO_STRING       = "to_string";           // POJO & Event only
    public static final String JAVA_CLONE_METHOD    = "java_clone";          // POJO & Event only


    /*---------------------------------------
     *
     * Common keys used during C generation
     *
     *---------------------------------------*/

    public static final String C_HEADER_FILENAME  = "c_header_filename";    // ex: [mysrc.h]
    public static final String C_HEADER_SUBDIR    = "c_header_subdir";      // ex: [libtest]/mysrc.h
    public static final String C_HEADER_IFDEF     = "c_header_ifdef";       // ex: #ifndef [MYSRC_H]
    public static final String C_INCLUDE_SECTION  = "c_include_sec";        // the "#include" section
    public static final String C_FREE_FORM        = "c_free_form";          // free-form injected code
    public static final String C_TYPE_DECS        = "c_type_defs";          // typedef definitions
    public static final String C_FUNCTION_DECS    = "c_function_decs";      // function declarations
    public static final String C_VARIABLE_DEFS    = "c_variable_defs";      // global variable definitions
    public static final String C_CONFIG_FLAG      = "c_config_flag";        // configuration flag


    /*---------------------------------------
     *
     * Keys used during POJO generation.  Most are
     * not language specific, but will contain the
     * language in the name if it is.
     *
     *---------------------------------------*/

    public static final String POJO_C_VARNAME       = "pojo_c_varname";
    public static final String POJO_CLASSNAME       = "pojo_classname";
    public static final String POJO_C_CLONE_WIRE    = "pojo_c_clone_wire";
    public static final String POJO_ENCODE_BODY     = "pojo_encode_body";
    public static final String POJO_DECODE_BODY     = "pojo_decode_body";
    public static final String ENUM_JAVA_VARS       = "enum_values";


    /*---------------------------------------
     *
     * Keys used during Event generation.  Most are
     * not language specific, but will contain the
     * language in the name if it is.
     *
     *---------------------------------------*/

    public static final String EVENT_CLASSNAME       = "event_classname";
    public static final String EVENT_SUPER_CLASS     = "eventSuperClass";
    public static final String EVENT_C_FUNCTION_BODY = "event_c_function_body";   // function body
    public static final String EVENT_CODE_HANDLER    = "event_code_handler";      // decode with the massive "switch" statment
    public static final String EVENT_DECODE_FUNCTION = "event_decode_function";   // function to call to decode an event
    public static final String APPLY_EVENT_CODE      = "apply_eventCode";
    public static final String DECODE_JAVA_EVENT     = "decode_java_event";     // Java-to-Java adapter
    public static final String SUPPORT_JAVA_EVENT    = "support_java_event";    // Java-to-Java adapter
    public static final String IMPLEMENTS_MARKER    = "IMPLEMENTS_MARKER";
    public static final String CONSTRUCTOR_MARKER   = "CONSTRUCTOR_MARKER";


    /*---------------------------------------
     *
     * Keys used during IPC Message generation.
     * Most are not language specific, but will
     * contain the language in the name if it is.
     *
     *---------------------------------------*/

    public static final String IPC_CODE_PREFIX      = "ipc_code_prefix";        // ex: APP_MGR_IPC_CODES_
    public static final String JAVA_CODES_INTERFACE = "java_codes_interface";
    public static final String HAND_MSG_CASE        = "handler_messages_case";
    public static final String HAND_MSG_DIRECT_CASE = "handler_direct_messages_case";
    public static final String HAND_ABSTACTS        = "handler_abstracts";
}

