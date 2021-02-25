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
 * sortedTest.c
 *
 * Tests to execute to validate icSortedLinkedList
 *
 * Author: jelderton - 8/24/15
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>

#include <icLog/logging.h>
#include "icTypes/icSortedLinkedList.h"
#include "sortedTest.h"

#define LOG_CAT     "logTEST"

typedef struct _sample
{
    int     num;
    char    label[50];
} sample;

static sample *createSample()
{
    sample *retVal = (sample *)malloc(sizeof(sample));
    memset(retVal, 0, sizeof(sample));
    return retVal;
}

static void printSample(sample *curr)
{
    icLogDebug(LOG_CAT, " sample num=%d label=%s", curr->num, curr->label);
}

static bool printIterator(void *item, void *arg)
{
    // typecast to 'sample'
    sample *curr = (sample *)item;
    printSample(curr);
    return true;
}

static void printList(icLinkedList *list)
{
    // loop through nodes of list, calling 'printIterator'
    //
    linkedListIterate(list, printIterator, NULL);
}

static int8_t sortToAddFunct(void *newItem, void *element)
{
    sample *there = (sample *)element;
    sample *add = (sample *)newItem;

    // looks strange, but Coverity complained about typecasting the return from strcmp()
    int rc = strcmp(add->label, there->label);
    if (rc < 0)
    {
        return (int8_t)-1;
    }
    else if (rc > 0)
    {
        return (int8_t)1;
    }
    return (int8_t)0;
}

static bool canAddToEmptyList()
{
    icSortedLinkedList *list = sortedLinkedListCreate();

    // add the first element
    //
    sample *s = createSample();
    strcpy(s->label, "A");
    sortedLinkedListAdd(list, s, sortToAddFunct);
    if (linkedListCount(list) == 0)
    {
        // fail
        //
        linkedListDestroy(list, NULL);
        return false;
    }

    // print
    //
    icLogDebug(LOG_CAT, "printing list from 'canAddToEmptyList'");
    printList(list);
    linkedListDestroy(list, NULL);

    return true;
}

static bool canAddToBeginningOfList()
{
    // add 2 items, to ensure the second goes in front
    //
    icSortedLinkedList *list = sortedLinkedListCreate();

    sample *b = createSample();
    strcpy(b->label, "B");
    sortedLinkedListAdd(list, b, sortToAddFunct);

    sample *a = createSample();
    strcpy(a->label, "A");
    sortedLinkedListAdd(list, a, sortToAddFunct);

    // print
    //
    icLogDebug(LOG_CAT, "printing list from 'canAddToBeginningOfList'");
    printList(list);
    linkedListDestroy(list, NULL);

    return true;
}

static bool canAddToEndOfList()
{
    // add 2 items, to ensure the second goes at the end
    //
    icSortedLinkedList *list = sortedLinkedListCreate();

    sample *a = createSample();
    strcpy(a->label, "A");
    sortedLinkedListAdd(list, a, sortToAddFunct);

    sample *b = createSample();
    strcpy(b->label, "B");
    sortedLinkedListAdd(list, b, sortToAddFunct);

    // print
    //
    icLogDebug(LOG_CAT, "printing list from 'canAddToEndOfList'");
    printList(list);
    linkedListDestroy(list, NULL);

    return true;
}

static bool canAddToMiddleOfList()
{
    // add 3 items, to ensure the second goes at the end
    //
    icSortedLinkedList *list = sortedLinkedListCreate();

    sample *b = createSample();
    strcpy(b->label, "B");
    sortedLinkedListAdd(list, b, sortToAddFunct);

    sample *c = createSample();
    strcpy(c->label, "C");
    sortedLinkedListAdd(list, c, sortToAddFunct);

    sample *a = createSample();
    strcpy(a->label, "A");
    sortedLinkedListAdd(list, a, sortToAddFunct);

    // print
    //
    icLogDebug(LOG_CAT, "printing list from 'canAddToMiddleOfList'");
    printList(list);
    linkedListDestroy(list, NULL);

    return true;
}


/*
 * main 'test' driver
 */
bool runSortedTests()
{
    if (canAddToEmptyList() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canAddToEmptyList");
        return false;
    }

    if (canAddToBeginningOfList() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canAddToBeginningOfList");
        return false;
    }

    if (canAddToEndOfList() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canAddToEndOfList");
        return false;
    }

    if (canAddToMiddleOfList() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canAddToMiddleOfList");
        return false;
    }

    return true;
}