/*)
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2017 RDK Management
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
*/

#include "ansc_platform.h"
#include "ccsp_base_api.h"
#include <libparodus.h>
#include "cJSON.h"
#include <sysevent/sysevent.h>
#include <syscfg/syscfg.h>
#include <ctype.h>
#include <math.h>

#include "common.h"
#include <icLog/logging.h>



/*----------------------------------------------------------------------------*/
/*                                   Macros                                   */
/*----------------------------------------------------------------------------*/
#define CONTENT_TYPE_JSON               "application/json"

#if defined(_COSA_BCM_MIPS_)
#define DEVICE_MAC                      "Device.DPoE.Mac_address"
#else
#define DEVICE_MAC                      "Device.X_CISCO_COM_CableModem.MACAddress"
#endif

#define CLIENT_PORT_NUM                 6670
#define MAX_PARAMETERNAME_LEN           512
#define DEVICE_PROPS_FILE               "/etc/device.properties"
#define ETH_WAN_STATUS_PARAM            "Device.Ethernet.X_RDKCENTRAL-COM_WAN.Enabled"
#define RDKB_ETHAGENT_COMPONENT_NAME    "com.cisco.spvtg.ccsp.ethagent"
#define RDKB_ETHAGENT_DBUS_PATH         "/com/cisco/spvtg/ccsp/ethagent"
#define ERT_SUBSYSTEM                   "eRT." 
#define MAX_HEALTH_CHECK_RETRIES        60


/*----------------------------------------------------------------------------*/
/*                             File scoped typedefs                           */
/*----------------------------------------------------------------------------*/
typedef struct _notify_params
{
    int activation_code;
    char * activation_status;
    char * partner_id;
    char * trace_id;
    char * xbo_account_id;
} notify_params_t;


/*----------------------------------------------------------------------------*/
/*                            File scoped variables                           */
/*----------------------------------------------------------------------------*/
static pthread_mutex_t device_mac_mutex = PTHREAD_MUTEX_INITIALIZER;
static char deviceMAC[32]={'\0'};
static libpd_instance_t xhfw_instance;


/*----------------------------------------------------------------------------*/
/*                              External variables                            */
/*----------------------------------------------------------------------------*/
extern ANSC_HANDLE bus_handle;


/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/
static void *connectParodus(void *arg);
static void *sendNotification(void *arg);
static void getDeviceMac(void);
static void macToLower(char macValue[]);
static char *getParodusUrl(void);
static void waitForEthAgentComponentReady(void);
static int  checkEthernetWanStatus(void);
static void checkComponentHealthStatus(const char *compName, const char *dbusPath, char *status, int *retStatus);
static void freeNotifyWrpMsg(wrp_msg_t *wrp_msg);
static void freeNotifyParams(notify_params_t *param);

int s_sysevent_connect(token_t *out_se_token);


/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
void Init_Parodus_Task(void)
{
    int err = 0;
    pthread_t parodusThreadId;

    err = pthread_create(&parodusThreadId, NULL, connectParodus, NULL);
    if (err != 0)
    {
        icLogError(LOG_TAG, "Error creating messages thread :[%s]", strerror(err));
    }
    else
    {
        icLogInfo(LOG_TAG, "connectParodus() thread created Successfully");
    }
}

void Send_Notification_Task(int activation_code,
                            const char * activation_status,
                            const char * partner_id,
                            const char * trace_id,
                            const char * xbo_account_id)
{
    int err = 0;
    pthread_t NotificationThreadId;
    notify_params_t *args = NULL;

    args = (notify_params_t *)calloc(1,sizeof(notify_params_t));
    if(args != NULL)
    {
        args->activation_code = activation_code;

        if (activation_status != NULL)
        {
            args->activation_status = strdup(activation_status);
        }

        if (partner_id != NULL)
        {
            args->partner_id = strdup(partner_id);
        }

        if (trace_id != NULL)
        {
            args->trace_id = strdup(trace_id);
        }

        if (xbo_account_id != NULL)
        {
            args->xbo_account_id = strdup(xbo_account_id);
        }

        err = pthread_create(&NotificationThreadId, NULL, sendNotification, (void *) args);
        if (err != 0)
        {
            icLogError(LOG_TAG, "Error creating Notification thread :[%s]", strerror(err));
            freeNotifyParams(args);
        }
        else
        {
            icLogInfo(LOG_TAG, "Notification thread created Successfully");
        }
    }
}


/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
static char *getParodusUrl(void)
{
    char *url = NULL;

    FILE *fp = fopen(DEVICE_PROPS_FILE, "r");
    if ( NULL != fp )
    {
        char buf[ 1024 ] = { 0 };
        while ( fgets(buf, sizeof(buf), fp) != NULL )
        {
            char *value = strstr(buf, "PARODUS_URL=");
            if ( value != NULL )
            {
                // Strip off any carriage return / newline
                buf[strcspn(buf, "\r\n")] = '\0';

                value = value + strlen("PARODUS_URL=");
                url = strdup(value);
                break;
            }
        }

        fclose(fp);
    }
    else
    {
        icLogError(LOG_TAG, "Failed to open device.properties file: %s", DEVICE_PROPS_FILE);
    }

    if ( NULL == url )
    {
        icLogError(LOG_TAG, "parodus url is not present in device.properties file");
    }

    return url;
}

static void waitForEthAgentComponentReady(void)
{
    char status[32] = {'\0'};
    int count = 0;
    int ret = -1;
    while(1)
    {
        checkComponentHealthStatus(RDKB_ETHAGENT_COMPONENT_NAME, RDKB_ETHAGENT_DBUS_PATH, status, &ret);
        if(ret == CCSP_SUCCESS && (strcmp(status, "Green") == 0))
        {
            icLogInfo(LOG_TAG, "%s component health is %s, continue", RDKB_ETHAGENT_COMPONENT_NAME, status);
            break;
        }
        else
        {
            count++;
            if(count > MAX_HEALTH_CHECK_RETRIES)
            {
                icLogInfo(LOG_TAG, "%s component Health check failed (ret:%d), continue",RDKB_ETHAGENT_COMPONENT_NAME, ret);
                break;
            }
            if(count%5 == 0)
            {
                icLogInfo(LOG_TAG, "%s component Health, ret:%d, waiting", RDKB_ETHAGENT_COMPONENT_NAME, ret);
            }
            sleep(5);
        }
    }
}

static void checkComponentHealthStatus(const char *compName, const char *dbusPath, char *status, int *retStatus)
{
    int ret = 0, val_size = 0;
    parameterValStruct_t **parameterval = NULL;
    char *parameterNames[1] = {};
    char tmp[MAX_PARAMETERNAME_LEN] = {'\0'};
    char str[MAX_PARAMETERNAME_LEN] = {'\0'};

    snprintf(tmp, sizeof(tmp), "%s.%s", compName, "Health");
    parameterNames[0] = tmp;

    snprintf(str, sizeof(str), "%s%s", ERT_SUBSYSTEM, compName);
    icLogTrace(LOG_TAG, "checkComponenHealhStatus(): %s", str);

    ret = CcspBaseIf_getParameterValues(bus_handle, str, dbusPath,  parameterNames, 1, &val_size, &parameterval);
    icLogTrace(LOG_TAG, "checkComponenHealhStatus(): ret=%d, val_size=%d", ret, val_size);
    if ((ret == CCSP_SUCCESS) && (parameterval != NULL) && (val_size > 0))
    {
        strcpy(status, parameterval[0]->parameterValue);
        icLogTrace(LOG_TAG, "checkComponenHealhStatus(): status=%s", status);
    }
    free_parameterValStruct_t (bus_handle, val_size, parameterval);

    *retStatus = ret;
}

static int checkEthernetWanStatus()
{
    int ret = -1, size =0, val_size =0;
    char compName[MAX_PARAMETERNAME_LEN] = { '\0' };
    char dbusPath[MAX_PARAMETERNAME_LEN] = { '\0' };
    parameterValStruct_t **parameterval = NULL;
    const char *getList[] = {ETH_WAN_STATUS_PARAM};
    componentStruct_t **ppComponents = NULL;
    char dst_pathname_cr[256] = {'\0'};
    char isEthEnabled[64]={'\0'};

    if(0 == syscfg_init())
    {
        if( 0 == syscfg_get( NULL, "eth_wan_enabled", isEthEnabled, sizeof(isEthEnabled)) && (isEthEnabled[0] != '\0' && strncmp(isEthEnabled, "true", strlen("true")) == 0))
        {
            icLogInfo(LOG_TAG, "Ethernet WAN is enabled");
            ret = CCSP_SUCCESS;
        }
    }
    else
    {
        waitForEthAgentComponentReady();
        snprintf(dst_pathname_cr, sizeof(dst_pathname_cr), "%s%s", ERT_SUBSYSTEM, CCSP_DBUS_INTERFACE_CR);
        ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle, dst_pathname_cr, ETH_WAN_STATUS_PARAM, "", &ppComponents, &size);
        if ( ret == CCSP_SUCCESS && size >= 1)
        {
            strncpy(compName, ppComponents[0]->componentName, sizeof(compName)-1);
            strncpy(dbusPath, ppComponents[0]->dbusPath, sizeof(dbusPath)-1);
        }
        else
        {
            CcspTraceError(("Failed to get component for %s ret: %d\n",ETH_WAN_STATUS_PARAM,ret));
        }
        free_componentStruct_t(bus_handle, size, ppComponents);

        if(strlen(compName) != 0 && strlen(dbusPath) != 0)
        {
            ret = CcspBaseIf_getParameterValues(bus_handle, compName, dbusPath, getList, 1, &val_size, &parameterval);
            if(ret == CCSP_SUCCESS && val_size > 0)
            {
                if(parameterval[0]->parameterValue != NULL && strncmp(parameterval[0]->parameterValue, "true", strlen("true")) == 0)
                {
                    CcspTraceInfo(("Ethernet WAN is enabled\n"));
                    ret = CCSP_SUCCESS;
                }
                else
                {
                    CcspTraceInfo(("Ethernet WAN is disabled\n"));
                    ret = CCSP_FAILURE;
                }
            }
            else
            {
                CcspTraceError(("Failed to get values for %s ret: %d\n",getList[0],ret));
            }
            free_parameterValStruct_t(bus_handle, val_size, parameterval);
        }
    }
    return ret;
}

static void getDeviceMac()
{
    int retryCount = 0;
    while(!strlen(deviceMAC))
    {
        pthread_mutex_lock(&device_mac_mutex);

        int ret = -1, size =0, val_size =0,cnt =0;
        char compName[MAX_PARAMETERNAME_LEN] = { '\0' };
        char dbusPath[MAX_PARAMETERNAME_LEN] = { '\0' };
        parameterValStruct_t **parameterval = NULL;
        const char *getList[] = {DEVICE_MAC};
        componentStruct_t **        ppComponents = NULL;
        char dst_pathname_cr[256] = {0};
        token_t  token;
        int fd = s_sysevent_connect(&token);
        char deviceMACValue[32] = { '\0' };
        char isEthEnabled[64]={'\0'};

        if (strlen(deviceMAC))
        {
            pthread_mutex_unlock(&device_mac_mutex);
            break;
        }

        if  (CCSP_SUCCESS == checkEthernetWanStatus() && sysevent_get(fd, token, "eth_wan_mac", deviceMACValue, sizeof(deviceMACValue)) == 0 && deviceMACValue[0] != '\0')
        {
            macToLower(deviceMACValue);
            retryCount = 0;
        }
        else
        {
            sprintf(dst_pathname_cr, "%s%s", "eRT.", CCSP_DBUS_INTERFACE_CR);
            ret = CcspBaseIf_discComponentSupportingNamespace(bus_handle, dst_pathname_cr, DEVICE_MAC, "", &ppComponents, &size);
            if ( ret == CCSP_SUCCESS && size >= 1)
            {
                strncpy(compName, ppComponents[0]->componentName, sizeof(compName)-1);
                strncpy(dbusPath, ppComponents[0]->dbusPath, sizeof(dbusPath)-1);
            }
            else
            {
                CcspTraceError(("Failed to get component for %s ret: %d\n",DEVICE_MAC,ret));
                retryCount++;
            }
            free_componentStruct_t(bus_handle, size, ppComponents);

            if(strlen(compName) != 0 && strlen(dbusPath) != 0)
            {
                ret = CcspBaseIf_getParameterValues(bus_handle,
                        compName, dbusPath,
                        getList,
                        1, &val_size, &parameterval);

                if(ret == CCSP_SUCCESS)
                {
                    for (cnt = 0; cnt < val_size; cnt++)
                    {
                        CcspTraceDebug(("parameterval[%d]->parameterName : %s\n",cnt,parameterval[cnt]->parameterName));
                        CcspTraceDebug(("parameterval[%d]->parameterValue : %s\n",cnt,parameterval[cnt]->parameterValue));
                        CcspTraceDebug(("parameterval[%d]->type :%d\n",cnt,parameterval[cnt]->type));
                    }
                    macToLower(parameterval[0]->parameterValue);
                    retryCount = 0;
                }
                else
                {
                    CcspTraceError(("Failed to get values for %s ret: %d\n",getList[0],ret));
                    retryCount++;
                }
                free_parameterValStruct_t(bus_handle, val_size, parameterval);
            }
        }

        if(retryCount == 0)
        {
            CcspTraceInfo(("deviceMAC is %s\n",deviceMAC));
            pthread_mutex_unlock(&device_mac_mutex);
            break;
        }
        else
        {
            if(retryCount > 5 )
            {
                CcspTraceError(("Unable to get CM Mac after %d retry attempts..\n", retryCount));
                pthread_mutex_unlock(&device_mac_mutex);
                break;
            }
            else
            {
                CcspTraceError(("Failed to GetValue for MAC. Retrying...retryCount %d\n", retryCount));
                pthread_mutex_unlock(&device_mac_mutex);
                sleep(10);
            }
        }
    }
}

static void macToLower(char macValue[])
{
    int i = 0;
    int j;
    char *token[32]={'\0'};
    char tmp[32]={'\0'};
    strncpy(tmp, macValue,sizeof(tmp)-1);
    token[i] = strtok(tmp, ":");
    if(token[i]!=NULL)
    {
        strncpy(deviceMAC, token[i],sizeof(deviceMAC)-1);
        deviceMAC[31]='\0';
        i++;
    }
    while ((token[i] = strtok(NULL, ":")) != NULL)
    {
        strncat(deviceMAC, token[i],sizeof(deviceMAC)-1);
        deviceMAC[31]='\0';
        i++;
    }
    deviceMAC[31]='\0';
    for(j = 0; deviceMAC[j]; j++)
    {
        deviceMAC[j] = tolower(deviceMAC[j]);
    }
}

static void* connectParodus(void *arg)
{
    int backoffRetryTime = 0;
    int backoff_max_time = 5;
    int max_retry_sleep;
    int c =2;   //Retry Backoff count shall start at c=2 & calculate 2^c - 1.
    int retval=-1;
    char *parodus_url = NULL;

    pthread_detach(pthread_self());

    max_retry_sleep = (int) pow(2, backoff_max_time) -1;
    icLogInfo(LOG_TAG, "max_retry_sleep is %d", max_retry_sleep );

    parodus_url = getParodusUrl();
    icLogInfo(LOG_TAG, "parodus_url is %s", parodus_url );

    libpd_cfg_t cfg = { .service_name = "xhfw",
                        .receive = false,
                        .keepalive_timeout_secs = 0,
                        .parodus_url = parodus_url,
                        .client_url = NULL
                      };

    icLogInfo(LOG_TAG, "Configurations => service_name : %s parodus_url : %s client_url : %s", cfg.service_name, cfg.parodus_url, cfg.client_url );

    while(1)
    {
        if(backoffRetryTime < max_retry_sleep)
        {
            backoffRetryTime = (int) pow(2, c) -1;
        }

        icLogInfo(LOG_TAG, "New backoffRetryTime value calculated as %d seconds", backoffRetryTime);
        int ret = libparodus_init (&xhfw_instance, &cfg);

        if(ret ==0)
        {
            icLogInfo(LOG_TAG, "Init for parodus Success..!!");
            break;
        }
        else
        {
            icLogError(LOG_TAG, "Init for parodus (url %s) failed: '%s'", parodus_url, libparodus_strerror(ret));
            if( NULL == parodus_url )
            {
                parodus_url = getParodusUrl();
                cfg.parodus_url = parodus_url;
            }
            sleep(backoffRetryTime);
            c++;

            if(backoffRetryTime == max_retry_sleep)
            {
                c = 2;
                backoffRetryTime = 0;
                icLogInfo(LOG_TAG, "backoffRetryTime reached max value, reseting to initial value");
            }
        }

        icLogInfo(LOG_TAG, "shutdown parodus");
        retval = libparodus_shutdown(&xhfw_instance);
    }

    return (void*)retval;
}

static void* parodus_receive_wait(void *vp)
{
    int rtn;
    wrp_msg_t *wrp_msg;

    icLogInfo(LOG_TAG, "parodus_receive_wait");
    while( 1 ) {
        rtn = libparodus_receive(xhfw_instance, &wrp_msg, 2000);
        icLogInfo(LOG_TAG, "    rtn = %d", rtn);
        if( 0 == rtn ) {
            icLogInfo(LOG_TAG, "Got something from parodus.");
        } else if( 1 == rtn || 2 == rtn ) {
            icLogInfo(LOG_TAG, "Timed out or message closed.");
            continue;
        } else {
            icLogError(LOG_TAG, "Libparodus failed to receive message: '%s'", libparodus_strerror(rtn));
        }
        if( NULL != wrp_msg ) {
            free(wrp_msg);
        }
        sleep(5);
    }
    libparodus_shutdown(&xhfw_instance);
    icLogInfo(LOG_TAG, "End of parodus_upstream");
    return 0;
}

void* sendNotification(void* pValue)
{
    notify_params_t *msg        = (notify_params_t *)pValue;
    wrp_msg_t *notif_wrp_msg    = NULL;
    int retry_count             = 0;
    int sendStatus              = -1;
    int backoffRetryTime        = 0;

    //Retry Backoff count shall start at c=2 & calculate 2^c - 1.
    int c                       = 2;

    char source[MAX_PARAMETERNAME_LEN] = {'\0'};
    char dest[MAX_PARAMETERNAME_LEN] = {'\0'};

    cJSON *notifyPayload        = cJSON_CreateObject();
    char *notifyPayloadString   = NULL;
    char *temp                  = NULL;

    pthread_detach(pthread_self());

    getDeviceMac();
    if(strlen(deviceMAC) == 0)
    {
        icLogError(LOG_TAG, "deviceMAC is NULL, failed to send Notification");
    }
    else
    {
        icLogInfo(LOG_TAG, "deviceMAC: %s",deviceMAC);
        snprintf(source, sizeof(source), "mac:%s", deviceMAC);

        if(notifyPayload != NULL)
        {
            cJSON_AddStringToObject(notifyPayload,"device_id", source);

            cJSON_AddStringToObject(notifyPayload,"status", "xhfw-status");

            cJSON *statusObj = cJSON_CreateObject();
            if(statusObj != NULL)
            {
                if (msg->activation_code >= 0)
                {
                    cJSON_AddNumberToObject(statusObj, "activation-code", msg->activation_code);
                }

                if (msg->activation_status != NULL)
                {
                    cJSON_AddStringToObject(statusObj, "activation-status", msg->activation_status);
                }

                if (msg->partner_id != NULL)
                {
                    cJSON_AddStringToObject(statusObj, "partner", msg->partner_id);
                }

                if (msg->trace_id != NULL)
                {
                    cJSON_AddStringToObject(statusObj, "trace-id", msg->trace_id);
                }

                if (msg->xbo_account_id != NULL)
                {
                    cJSON_AddStringToObject(statusObj, "xbo-account-id", msg->xbo_account_id);
                }

                cJSON_AddItemToObject(notifyPayload, "xhfw-status", statusObj);
            }

            notifyPayloadString = cJSON_PrintUnformatted(notifyPayload);
            icLogInfo(LOG_TAG, "payload: %s", notifyPayloadString);
        }

        snprintf(source, sizeof(source), "mac:%s/xhfw", deviceMAC);

        notif_wrp_msg = (wrp_msg_t *)calloc(1,sizeof(wrp_msg_t));
        if(notif_wrp_msg != NULL)
        {
            notif_wrp_msg->msg_type = WRP_MSG_TYPE__EVENT;
            notif_wrp_msg->u.event.source = strdup(source);
            icLogInfo(LOG_TAG, "source: %s",notif_wrp_msg ->u.event.source);

            snprintf(dest,sizeof(dest),"event:device-status/mac:%s/xhfw-status", deviceMAC);

            notif_wrp_msg ->u.event.dest = strdup(dest);
            icLogInfo(LOG_TAG, "destination: %s", notif_wrp_msg ->u.event.dest);
            notif_wrp_msg->u.event.content_type = strdup(CONTENT_TYPE_JSON);
            icLogInfo(LOG_TAG, "content_type: %s",notif_wrp_msg->u.event.content_type);
            if(notifyPayloadString != NULL)
            {
                notif_wrp_msg ->u.event.payload = (void *) notifyPayloadString;
                notif_wrp_msg ->u.event.payload_size = strlen(notifyPayloadString);
            }

            while(retry_count<=3)
            {
                backoffRetryTime = (int) pow(2, c) -1;

                sendStatus = libparodus_send(xhfw_instance, notif_wrp_msg );

                if(sendStatus == 0)
                {
                    retry_count = 0;
                    icLogInfo(LOG_TAG, "Notification successfully sent to parodus");
                    break;
                }
                else
                {
                    icLogError(LOG_TAG, "Failed to send Notification: '%s', retrying ....",libparodus_strerror(sendStatus));
                    icLogInfo(LOG_TAG, "sendNotification() backoffRetryTime %d seconds", backoffRetryTime);
                    sleep(backoffRetryTime);
                    c++;
                    retry_count++;
                }
            }
            icLogInfo(LOG_TAG, "sendStatus is %d",sendStatus);
        }
    }

    freeNotifyWrpMsg(notif_wrp_msg);
    freeNotifyParams(msg);
    if(notifyPayloadString != NULL)
    {
        free(notifyPayloadString);
    }
    if (notifyPayload != NULL)
    {
        cJSON_Delete(notifyPayload);
    }
    return NULL;
}

void freeNotifyWrpMsg(wrp_msg_t *wrp_msg)
{
    if (wrp_msg != NULL)
    {
        if (wrp_msg->u.event.source != NULL)
        {
            free(wrp_msg->u.event.source);
        }
        if (wrp_msg->u.event.dest != NULL)
        {
            free(wrp_msg->u.event.dest);
        }
        if (wrp_msg->u.event.content_type != NULL)
        {
            free(wrp_msg->u.event.content_type);
        }
        free(wrp_msg);
    }
}

void freeNotifyParams(notify_params_t *param)
{
    if(param != NULL)
    {
        if(param->activation_status != NULL)
        {
            free(param->activation_status);
            param->activation_status = NULL;
        }
        if(param->partner_id != NULL)
        {
            free(param->partner_id);
            param->partner_id = NULL;
        }
        if(param->trace_id != NULL)
        {
            free(param->trace_id);
            param->trace_id = NULL;
        }
        if(param->xbo_account_id != NULL)
        {
            free(param->xbo_account_id);
            param->xbo_account_id = NULL;
        }
        free(param);
    }
}

const char *rdk_logger_module_fetch(void)
{
    return "LOG.RDK.WEBPA";
}

