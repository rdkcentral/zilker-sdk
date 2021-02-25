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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <icTypes/icLinkedList.h>
#include <icLog/logging.h>
#include "linkedTest.h"

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

static icLinkedList *makePopulatedList(int entryCount)
{
    icLinkedList *list = linkedListCreate();

    int i = 0;
    sample *item = NULL;
    for (i = 0 ; i < entryCount ; i++)
    {
        item = createSample();
        item->num = (i + 1);
        sprintf(item->label, "testing %d", item->num);
        linkedListAppend(list, item);
    }
    return list;
}

/*
 * test creating a linked list (0 itmes)
 */
bool canCreate()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    // create then destroy a icLinkedList
    //
    icLinkedList *list = linkedListCreate();
    linkedListDestroy(list, NULL);

    return true;
}

/*
 * test adding items to the end of the linked list
 */
bool canAppend()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    icLinkedList *list = linkedListCreate();
    int count = 0;
    int i = 0;
    sample *item = NULL;

    // make initial entry
    //
    item = createSample();
    item->num = 0;
    strcpy(item->label, "testing 1,2,3");
    linkedListAppend(list, item);

    // check size
    //
    count = linkedListCount(list);
    icLogDebug(LOG_CAT, "  %s - appended 1 entry, count = %d", __FUNCTION__, count);
    if (count != 1)
    {
        linkedListDestroy(list, NULL);
        return false;
    }

    // add 4 more
    //
    for (i = 0 ; i < 4 ; i++)
    {
        item = createSample();
        item->num = (i + 1);
        strcpy(item->label, "testing 1,2,3");
        linkedListAppend(list, item);
    }

    // check size
    //
    count = linkedListCount(list);
    icLogDebug(LOG_CAT, "  %s - appended 5 entries, count = %d", __FUNCTION__, count);
    if (count != 5)
    {
        linkedListDestroy(list, NULL);
        return false;
    }

    printList(list);
    linkedListDestroy(list, NULL);

    return true;
}

/*
 * prepend items to make sure they are inserted at the front properly
 */
bool canPrepend()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    // test creating a linked list (0 itmes)
    //
    icLinkedList *list = linkedListCreate();

    int count = 0;
    int i = 0;
    sample *item = NULL;

    // make initial entry
    //
    item = createSample();
    item->num = 0;
    strcpy(item->label, "testing 1,2,3");
    linkedListPrepend(list, item);

    // check size
    //
    count = linkedListCount(list);
    icLogDebug(LOG_CAT, "  %s - prepended 1 entry, count = %d", __FUNCTION__, count);
    if (count != 1)
    {
        linkedListDestroy(list, NULL);
        return false;
    }

    // add 4 more
    //
    for (i = 0 ; i < 4 ; i++)
    {
        item = createSample();
        item->num = (i + 1);
        strcpy(item->label, "testing 1,2,3");
        linkedListPrepend(list, item);
    }

    // check size
    //
    count = linkedListCount(list);
    icLogDebug(LOG_CAT, "  %s - prepended 5 entries, count = %d", __FUNCTION__, count);
    if (count != 5)
    {
        linkedListDestroy(list, NULL);
        return false;
    }

    printList(list);
    linkedListDestroy(list, NULL);

    return true;
}

bool sampleSearch(void *searchVal, void *item)
{
    // typecast to 'sample'
    sample *curr = (sample *)item;
    int search = *((int *)searchVal);

    // looking for matching 'num'
    if (search == curr->num)
    {
        return true;
    }

    return false;
}

/*
 * create list of 5 items, then find some of them
 */
bool canFind()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    // make list of 5 items
    //
    int i = 0;
    icLinkedList *list = makePopulatedList(5);
    printList(list);

    // try to find the one with "num=2"
    //
    i = 2;
    void *found = linkedListFind(list, &i, sampleSearch);
    if (found == NULL)
    {
        icLogWarn(LOG_CAT, "unable to locate sample #2");
        linkedListDestroy(list, NULL);
        return false;
    }
    printSample((sample *)found);

    // try to find the one with "num=2"
    //
    i = 5;
    found = linkedListFind(list, &i, sampleSearch);
    if (found == NULL)
    {
        icLogWarn(LOG_CAT, "unable to locate sample #5");
        linkedListDestroy(list, NULL);
        return false;
    }
    printSample((sample *)found);

    linkedListDestroy(list, NULL);

    return true;
}

/*
 *
 */
bool canNotFind()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    // make list of 5 items
    //
    int i = 0;
    icLinkedList *list = makePopulatedList(5);

    // call find, but something not there
    //
    i = 102;
    void *found = linkedListFind(list, &i, sampleSearch);
    if (found != NULL)
    {
        icLogWarn(LOG_CAT, "found sample that we should not have");
        linkedListDestroy(list, NULL);
        return false;
    }

    linkedListDestroy(list, NULL);
    return true;
}

/*
 *
 */
bool canDelete()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    // make list of 5 items
    //
    int i = 0;
    icLinkedList *list = makePopulatedList(5);

    // delete #4
    i = 4;
    if (linkedListDelete(list, &i, sampleSearch, NULL) != true)
    {
        icLogWarn(LOG_CAT, "unable to delete node #4");
        linkedListDestroy(list, NULL);
        return false;
    }
    printList(list);

    // delete #0
    i = 1;
    if (linkedListDelete(list, &i, sampleSearch, NULL) != true)
    {
        icLogWarn(LOG_CAT, "unable to delete node #1");
        linkedListDestroy(list, NULL);
        return false;
    }
    printList(list);

    // delete #5
    i = 5;
    if (linkedListDelete(list, &i, sampleSearch, NULL) != true)
    {
        icLogWarn(LOG_CAT, "unable to delete node #5");
        linkedListDestroy(list, NULL);
        return false;
    }
    printList(list);

    linkedListDestroy(list, NULL);
    return true;
}

/*
 * test alternative iterator
 */
bool canIterateAlternative()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    // make list of 5 items
    //
    int i = 0;
    icLinkedList *list = makePopulatedList(5);
//    printList(list);

    // get an iterator
    //
    icLinkedListIterator *loop = linkedListIteratorCreate(list);
    while (linkedListIteratorHasNext(loop) == true)
    {
        void *item = linkedListIteratorGetNext(loop);
        sample *curr = (sample *)item;
        printSample(curr);
        i++;
    }

    sample *nonExistent = linkedListIteratorGetNext(loop);
    if (nonExistent != NULL)
    {
        icLogError(LOG_CAT, "Iterator returns items after traversal");
        return false;
    }

    // done with loop, see if we got the correct number
    //
    if (i != 5)
    {
        icLogWarn(LOG_CAT, "unable to iterate all nodes properly, got %d expected 5", i);
        linkedListIteratorDestroy(loop);
        linkedListDestroy(list, NULL);
        return false;
    }

    linkedListIteratorDestroy(loop);
    linkedListDestroy(list, NULL);

    return true;
}


/*
 * test deleting an item from within the iterator
 */
bool canDeleteFromIterator()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    // make list of 5 items
    //
    int i = 0;
    icLinkedList *list = makePopulatedList(5);

    // get an iterator
    //
    icLinkedListIterator *loop = linkedListIteratorCreate(list);

    // make sure we cannot delete before the first call to get next
    //
    if (linkedListIteratorDeleteCurrent(loop, NULL) == true)
    {
        icLogWarn(LOG_CAT, "able to delete prematurely from iterator!");
        linkedListIteratorDestroy(loop);
        linkedListDestroy(list, NULL);
        return false;
    }

    while (linkedListIteratorHasNext(loop) == true)
    {
        void *item = linkedListIteratorGetNext(loop);
        sample *curr = (sample *)item;
        printSample(curr);
        i++;

        // try first, middle & end
        if (i == 1 || i == 3 || i == 5)
        {
            // delete this item
            icLogDebug(LOG_CAT, "deleting item #%d", i);
            linkedListIteratorDeleteCurrent(loop, NULL);
        }
    }
    linkedListIteratorDestroy(loop);
    printList(list);

    // done with loop, see if we correcly removed 2 items
    //
    i = linkedListCount(list);
    if (i != 2)
    {
        icLogWarn(LOG_CAT, "unable to delete from iterator, got %d expected 2", i);
        linkedListDestroy(list, NULL);
        return false;
    }

    linkedListDestroy(list, NULL);
    return true;
}

static void *cloneItem(void *item, void *context)
{
    if (strcmp(context, "context") != 0)
    {
        icLogError(LOG_CAT, "Unexpected context object");
        return NULL;
    }

    sample *orig = (sample *)item;
    sample *copy = createSample();
    copy->num = orig->num;
    strcpy(copy->label,orig->label);

    return copy;
}

bool canDeepClone()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    // make list of 5 items
    //
    char context[50] = "context";
    icLinkedList *list = makePopulatedList(5);
    icLinkedList *copy = linkedListDeepClone(list, cloneItem, context);
    if (linkedListCount(copy) != 5)
    {
        icLogError(LOG_CAT, "Deep cloned list does not contain all items");
        linkedListDestroy(list, NULL);
        linkedListDestroy(copy, NULL);
        return false;
    }

    bool retVal = true;
    icLinkedListIterator *origIter = linkedListIteratorCreate(list);
    icLinkedListIterator *copyIter = linkedListIteratorCreate(copy);
    while(linkedListIteratorHasNext(origIter) && linkedListIteratorHasNext(copyIter))
    {
        sample *origItem = linkedListIteratorGetNext(origIter);
        sample *copyItem = linkedListIteratorGetNext(copyIter);

        if (origItem->num != copyItem->num)
        {
            icLogError(LOG_CAT, "Copied item num does not match");
            retVal = false;
            break;
        }

        if (strcmp(origItem->label,copyItem->label) != 0)
        {
            icLogError(LOG_CAT, "Copied item label does not match");
            retVal = false;
            break;
        }
    }
    linkedListIteratorDestroy(origIter);
    linkedListIteratorDestroy(copyIter);
    linkedListDestroy(list, NULL);
    if (retVal == false)
    {
        linkedListDestroy(copy, NULL);
        return retVal;
    }

    // Test can still use after deleting of original list
    copyIter = linkedListIteratorCreate(copy);
    while(linkedListIteratorHasNext(copyIter))
    {
        sample *copyItem = linkedListIteratorGetNext(copyIter);
        icLogDebug(LOG_CAT, "at item #%d with label %s", copyItem->num, copyItem->label);

    }
    linkedListIteratorDestroy(copyIter);
    linkedListDestroy(copy, NULL);

    return true;
}

bool canShallowCloneList()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    icLinkedList *list = makePopulatedList(5);
    icLinkedList *clonedList = linkedListClone(list);

    if(linkedListIsClone(clonedList) == false)
    {
        icLogError(LOG_CAT, "Shallow cloned list is not marked as cloned");
        linkedListDestroy(list, NULL);
        linkedListDestroy(clonedList, NULL);
        return false;
    }

    if (linkedListCount(clonedList) != 5)
    {
        icLogError(LOG_CAT, "Shallow cloned list does not contain all items");
        linkedListDestroy(list, NULL);
        linkedListDestroy(clonedList, NULL);
        return false;
    }

    icLinkedListIterator *origIter = linkedListIteratorCreate(list);
    icLinkedListIterator *copyIter = linkedListIteratorCreate(clonedList);

    bool retVal = true;
    while(linkedListIteratorHasNext(origIter) && linkedListIteratorHasNext(copyIter))
    {
        sample *origItem = linkedListIteratorGetNext(origIter);
        sample *copyItem = linkedListIteratorGetNext(copyIter);

        if (origItem->num != copyItem->num)
        {
            icLogError(LOG_CAT, "Copied item num does not match");
            retVal = false;
            break;
        }

        if (origItem->label != copyItem->label)
        {
            icLogError(LOG_CAT, "Copied item label does not match");
            retVal = false;
            break;
        }
    }
    linkedListIteratorDestroy(origIter);
    linkedListIteratorDestroy(copyIter);

    linkedListDestroy(list, NULL);
    linkedListDestroy(clonedList, NULL);

    return retVal;
}

bool canGetElementAt()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    icLinkedList *list = makePopulatedList(5);
    uint32_t offset = 6;

    sample *retVal = linkedListGetElementAt(list, offset);
    if (retVal != NULL)
    {
        icLogError(LOG_CAT, "Got element from index outside of list size");
        linkedListDestroy(list, NULL);
        return false;
    }

    offset = 3;
    retVal = linkedListGetElementAt(list, offset);
    if (retVal == NULL)
    {
        icLogError(LOG_CAT, "Failed to remove valid element");
        linkedListDestroy(list, NULL);
        return false;
    }

    icLinkedListIterator *listIter = linkedListIteratorCreate(list);
    size_t i = 0;
    bool isCorrect = false;
    while(linkedListIteratorHasNext(listIter))
    {
        sample *element = linkedListIteratorGetNext(listIter);
        if (i == 3 && element == retVal)
        {
            isCorrect = true;
        }
        i++;
    }
    linkedListIteratorDestroy(listIter);

    bool rc = true;
    if (isCorrect == false)
    {
       icLogError(LOG_CAT, "Did not get the correct element");
       rc = false;
    }
    linkedListDestroy(list, NULL);

    return rc;
}

bool canRemovElementAt()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    icLinkedList *list = makePopulatedList(5);
    uint32_t offset = 6;

    sample *retVal = linkedListRemove(list, offset);
    if (retVal != NULL)
    {
        icLogError(LOG_CAT, "Removed index outside of list size");
        linkedListDestroy(list, NULL);
        return false;
    }

    offset = 3;
    sample *element = linkedListGetElementAt(list, offset);
    retVal = linkedListRemove(list, offset);
    if (retVal == NULL)
    {
        icLogError(LOG_CAT, "Failed to remove valid element");
        linkedListDestroy(list, NULL);
        return false;
    }

    if (linkedListCount(list) != 4)
    {
        icLogError(LOG_CAT, "Reported size is wrong, despite removing element");
        free(retVal);
        linkedListDestroy(list, NULL);
        return false;
    }

    if (element != retVal)
    {
        icLogError(LOG_CAT, "Wrong element returned");
        free(retVal);
        linkedListDestroy(list, NULL);
        return false;
    }

    free(retVal);
    linkedListDestroy(list, NULL);

    return true;
}

//Helper function for iterate. Adds seven to each element.
bool addSevenToAll(void *item, void *arg)
{
    sample *element = (sample*) item;

    if (element == NULL)
    {
        return false;
    }

    element->num += 7;
    return true;
}

bool canIterateList()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    icLinkedList *list = makePopulatedList(5);

    int numbers[5];
    icLinkedListIterator *iter = linkedListIteratorCreate(list);
    size_t i = 0;
    while(linkedListIteratorHasNext(iter))
    {
        sample *elem = linkedListIteratorGetNext(iter);
        if (elem != NULL)
        {
            numbers[i++] = elem->num;
        }
    }

    linkedListIteratorDestroy(iter);

    linkedListIterate(list, addSevenToAll, NULL);

    iter = linkedListIteratorCreate(list);
    i = 0;
    while(linkedListIteratorHasNext(iter))
    {
        sample *elem = linkedListIteratorGetNext(iter);
        if (elem != NULL)
        {
            if (elem->num != numbers[i]+7)
            {
                icLogError(LOG_CAT, "The iteration function didn't apply");
                return false;
            }
        }

        i++;
    }

    linkedListIteratorDestroy(iter);

    linkedListDestroy(list, NULL);

    return true;
}

bool scopeBoundIteratorIsNotLeaky()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    icLinkedList *list = makePopulatedList(5);

    for (int i = 0; i < 2; i++)
    {
        sbIcLinkedListIterator *iter = linkedListIteratorCreate(list);
        while (linkedListIteratorHasNext(iter))
        {
            void *testVal = linkedListIteratorGetNext(iter);
            if (!testVal)
            {
                puts("Test list returned NULL test value");
                return false;
            }
        }
    }

    linkedListDestroy(list, NULL);

    /* libasan will detect any leaks */
    return true;
}

bool canClearList()
{
    icLogDebug(LOG_CAT, "testing %s", __FUNCTION__);

    size_t size = 5;
    icLinkedList *list = makePopulatedList(size);
    //Will use later. Assume cloning is fully tested already
    icLinkedList *clonedList = linkedListClone(list);

    if (linkedListIsClone(clonedList) == false)
    {
        icLogError(LOG_CAT, "Cloned list is not marked as cloned");
        linkedListDestroy(list, NULL);
        linkedListDestroy(clonedList, NULL);
        return false;
    }

    icLinkedListIterator *iter = linkedListIteratorCreate(list);
    size_t count = 0;
    while(linkedListIteratorHasNext(iter))
    {
        linkedListIteratorGetNext(iter);
        count+=1;
    }
    linkedListIteratorDestroy(iter);

    if (size != linkedListCount(list) && count != size)
    {
        icLogError(LOG_CAT, "Sizes don't match during clear list");
        linkedListDestroy(list, NULL);
        linkedListDestroy(clonedList, NULL);
        return false;
    }

    linkedListClear(list, NULL);
    if (linkedListCount(list) != 0)
    {
        icLogError(LOG_CAT, "Cleared list still has size");
        linkedListDestroy(list, NULL);
        linkedListDestroy(clonedList, NULL);
        return false;
    }

    iter = linkedListIteratorCreate(list);
    count = 0;
    while(linkedListIteratorHasNext(iter))
    {
        linkedListIteratorGetNext(iter);
        count+=1;
    }
    linkedListIteratorDestroy(iter);

    if (count != 0)
    {
        icLogError(LOG_CAT, "Cleared list still has stuff");
        linkedListDestroy(list, NULL);
        linkedListDestroy(clonedList, NULL);
        return false;
    }

    linkedListClear(clonedList, NULL);
    if (linkedListIsClone(clonedList) != false)
    {
        icLogError(LOG_CAT, "Cloned list not updated after clear");
        linkedListDestroy(list, NULL);
        linkedListDestroy(clonedList, NULL);
        return false;
    }

    if (linkedListCount(clonedList) != 0)
    {
        icLogError(LOG_CAT, "Cleared cloned list still has size");
        linkedListDestroy(list, NULL);
        linkedListDestroy(clonedList, NULL);
        return false;
    }

    iter = linkedListIteratorCreate(clonedList);
    count = 0;
    while(linkedListIteratorHasNext(iter))
    {
        linkedListIteratorGetNext(iter);
        count+=1;
    }
    linkedListIteratorDestroy(iter);

    if (count != 0)
    {
        icLogError(LOG_CAT, "Cleared cloned list still has stuff");
        linkedListDestroy(list, NULL);
        linkedListDestroy(clonedList, NULL);
        return false;
    }

    linkedListDestroy(list, NULL);
    linkedListDestroy(clonedList, NULL);

    return true;
}

/*
 * main
 */
bool runLinkedTests()
{
    if (canCreate() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canCreate");
        return false;
    }

    if (canAppend() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canAppend");
        return false;
    }

    if (canPrepend() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canPrepend");
        return false;
    }

    if (canFind() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canFind");
        return false;
    }

    if (canNotFind() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canNotFind");
        return false;
    }

    if (canDelete() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canDelete");
        return false;
    }

    if (canIterateAlternative() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canIterateAlternative");
        return false;
    }

    if (canDeleteFromIterator() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canDeleteFromIterator");
        return false;
    }

    if (canDeepClone() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canDeepClone");
        return false;
    }

    if (canShallowCloneList() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canShallowClone");
        return false;
    }

    if (canGetElementAt() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canGetElementAt");
        return false;
    }

    if (canRemovElementAt() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canRemoveElementAt");
        return false;
    }

    if (canIterateList() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canIterateList");
        return false;
    }

    if (canClearList() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canClearList");
        return false;
    }

    scopeBoundIteratorIsNotLeaky();

    return true;
}
