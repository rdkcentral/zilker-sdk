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

/*-----------------------------------------------
 * troubleContainer.c
 *
 * container used internally by troubleState to hold
 * all of the known information about an individual trouble
 *
 * Author: jelderton -  3/3/19.
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include "trouble/troubleContainer.h"
#include "troubleContainer.h"

/*
 * create an empty troubleContainer
 */
troubleContainer *createTroubleContainer()
{
    troubleContainer *result = (troubleContainer *)calloc(1, sizeof(troubleContainer));
    result->persist = true;
    return result;
}

/*
 * deep clone a troubleContainer
 */
troubleContainer *cloneTroubleContainer(troubleContainer *container)
{
    // first the container object
    //
    troubleContainer *retVal = createTroubleContainer();

    // copy the persist flag
    //
    retVal->persist = container->persist;

    // now the event
    //
    if (container->event != NULL)
    {
        retVal->event = clone_troubleEvent(container->event);
    }

    // now the payload
    //
    retVal->payloadType = container->payloadType;
    switch (container->payloadType)
    {
        case TROUBLE_DEVICE_TYPE_ZONE:
        {
            if (container->extraPayload.zone != NULL)
            {
                // encode/decode the payload to effectively clone it
                cJSON *json = encodeSensorTroublePayload(container->extraPayload.zone);
                retVal->extraPayload.zone = decodeSensorTroublePayload(json);
                cJSON_Delete(json);
            }
            break;
        }

        case TROUBLE_DEVICE_TYPE_CAMERA:
        {
            if (container->extraPayload.camera != NULL)
            {
                // encode/decode the payload to effectively clone it
                cJSON *json = encodeCameraTroublePayload(container->extraPayload.camera);
                retVal->extraPayload.camera = decodeCameraTroublePayload(json);
                cJSON_Delete(json);
            }
            break;
        }

        case TROUBLE_DEVICE_TYPE_IOT:
        {
            if (container->extraPayload.device != NULL)
            {
                // encode/decode the payload to effectively clone it
                cJSON *json = encodeDeviceTroublePayload(container->extraPayload.device);
                retVal->extraPayload.device = decodeDeviceTroublePayload(json);
                cJSON_Delete(json);
            }
            break;
        }

        case TROUBLE_DEVICE_TYPE_NONE:
        default:
            // nothing to copy
            break;
    }

    return retVal;
}

/*
 * destroy a troubleConteiner
 */
void destroyTroubleContainer(troubleContainer *container)
{
    if (container != NULL)
    {
        // free the payload
        //
        switch (container->payloadType)
        {
            case TROUBLE_DEVICE_TYPE_ZONE:
                if (container->extraPayload.zone != NULL)
                {
                    sensorTroublePayloadDestroy(container->extraPayload.zone);
                    container->extraPayload.zone = NULL;
                }
                break;

            case TROUBLE_DEVICE_TYPE_CAMERA:
                if (container->extraPayload.camera != NULL)
                {
                    cameraTroublePayloadDestroy(container->extraPayload.camera);
                    container->extraPayload.camera = NULL;
                }
                break;

            case TROUBLE_DEVICE_TYPE_IOT:
                if (container->extraPayload.device != NULL)
                {
                    deviceTroublePayloadDestroy(container->extraPayload.device);
                    container->extraPayload.device = NULL;
                }
                break;

            case TROUBLE_DEVICE_TYPE_NONE:
            default:
                break;
        }

        // free the event
        //
        destroy_troubleEvent(container->event);
        container->event = NULL;

        // now the container itself
        //
        free(container);
    }
}

/*
 * 'linkedListItemFreeFunc' implementation used when deleting
 * troubleContainer objects from the linked list.
 */
void destroyTroubleContainerFromList(void *item)
{
    troubleContainer *obj = (troubleContainer *)item;
    destroyTroubleContainer(obj);
}
