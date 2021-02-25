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
 * icLinkedList.c
 *
 * Simplistic linked-list to provide the basic need for
 * dynamic Lists.  Probably temporary as we may switch
 * to utilize a 3rd-party implementation at some point.
 *
 * Supports operations such as:
 *   create a list
 *   append an item to the end of a list
 *   insert an item to the front of a list
 *   delete an item from the list
 *   destroy the list
 *
 * NOTE: this does not perform any mutex locking to
 *       allow for single-threaded usage without the
 *       overhead.  If locking is required, it should
 *       be performed by the caller
 *
 * Author: jelderton - 6/18/15
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>

#include "icTypes/icLinkedList.h"
#include "list.h"                   // common list node definition (shared with queue)

extern inline void linkedListIteratorDestroy__auto(icLinkedListIterator **iter);
extern inline void linkedListDestroy_generic__auto(icLinkedList **list);

// model the LinkedListIterator
//
struct _icLinkedListIterator
{
    icLinkedList  *head;        // list we're iterating through
    listNode      *curr;        // node to return in 'getNext'
    listNode      *prev;        // node returned in last call to 'getNext'.  used for 'delete'
};

/*-------------------------------*
 *
 *  icLinkedList
 *
 *-------------------------------*/

/*
 * create a new linked list.  will need to be released when complete
 *
 * @return a new LinkedList object
 * @see linkedListDestroy()
 */
icLinkedList *linkedListCreate()
{
    // allocate a 'listHead' object and return to the caller
    // this allows us to insert/delete at the real HEAD without
    // requiring the caller to change their pointer to the
    // logical linked list
    //
    icLinkedList *retVal = (icLinkedList *) malloc(sizeof(icLinkedList));
    if (retVal != NULL)
    {
        memset(retVal, 0, sizeof(icLinkedList));
        retVal->cloned = false;
    }

    return retVal;
}

/*
 * create a shallow-clone of an existing linked list.
 * will need to be released when complete, HOWEVER,
 * the items in the cloned list are not owned by it
 * and therefore should NOT be released.
 *
 * @see linkedListDestroy()
 * @see linkedListIsClone()
 */
icLinkedList *linkedListClone(icLinkedList *src)
{
    // Short circuit if NULL
    if (src == NULL)
    {
        return NULL;
    }

    // create the return struct
    //
    icLinkedList *retVal = linkedListCreate();
    if (retVal != NULL)
    {
        retVal->cloned = true;

        // walk the elements of 'src' and make a new container
        // in 'retVal' for each
        //
        listNode *ptr = src->first;
        while (ptr != NULL)
        {
            linkedListAppend(retVal, ptr->item);
            ptr = ptr->next;
        }
    }

    return retVal;
}

/*
 * create a deep-clone of an existing linked list.
 * will need to be released when complete.
 *
 * @see linkedListDestroy()
 */
icLinkedList *linkedListDeepClone(icLinkedList *src, linkedListCloneItemFunc cloneItemFunc, void *context)
{
    // create the return struct
    //
    icLinkedList *retVal = linkedListCreate();
    if (retVal != NULL)
    {
        // walk the elements of 'src' and make a new container
        // in 'retVal' for each
        //
        listNode *ptr = src->first;
        while (ptr != NULL)
        {
            linkedListAppend(retVal, cloneItemFunc(ptr->item, context));
            ptr = ptr->next;
        }
    }

    return retVal;
}

/*
 * destroy a linked list and cleanup memory.  note that
 * each 'item' will just be release via free() unless
 * a 'helper' is provided.
 * NOTE: items in the list are NOT RELEASED if this list is a 'clone' (regardless of 'helper' parameter)
 *
 * @param list - the LinkedList to delete
 * @param helper - optional, only needed if the items need a custom mechanism to release the memory
 *
 * @see linkedListClone()
 */
void linkedListDestroy(icLinkedList *list, linkedListItemFreeFunc helper)
{
    if (list == NULL)
    {
        return;
    }

    // iterate through the list, calling helper for each
    //
    listNode *ptr = list->first;
    while (ptr != NULL)
    {
        // free the element within the 'node' object
        // (note we skip if this is a cloned list)
        //
        if (list->cloned == false)
        {
            if (helper)
            {
                // use supplied callback function to clear mem
                helper(ptr->item);
            }
            else if (ptr->item != NULL)
            {
                // normal mem cleanup
                free(ptr->item);
            }
        }

        // get the next node before releasing this one
        //
        listNode *tmp = ptr->next;
        free(ptr);
        ptr = tmp;
    }

    // released each node & item, so now free the LinkedList
    //
    free(list);
}

/*
 * return true if this list was created via linkedListClone
 *
 * @see linkedListClone
 */
bool linkedListIsClone(icLinkedList *list)
{
    if (list == NULL)
    {
        return false;
    }
    return list->cloned;
}

/*
 * common implementation of the linkedListItemFreeFunc function that can be
 * used in situations where the 'item' is not freed at all.
 * generally used when the list contains pointers to functions
 */
void standardDoNotFreeFunc(void *item)
{
    // do nothing
}

/*
 * return the number of elements in the list
 *
 * @param list - the LinkedList to count
 */
uint16_t linkedListCount(icLinkedList *list)
{
    if (list == NULL)
    {
        return 0;
    }

    // assume the counter within the header is correct
    //
    return list->size;
}

/*
 * append 'item' to the end of the LinkedList
 * (ex: add to the end)
 *
 * @param list - the LinkedList to alter
 * @param item - the item to add
 * @return TRUE on success
 */
bool linkedListAppend(icLinkedList *list, void *item)
{
    if (list == NULL || item == NULL)
    {
        return false;
    }

    listNode *ptr = list->first;
    listNode *tail = NULL;

    // find tail (just skip 'counter' times?)
    //
    while (ptr != NULL)
    {
        tail = ptr;
        ptr = ptr->next;
    }

    // create the node and assign 'item' to it
    //
    listNode *node = createListNode();
    node->item = item;

    // have tail point to the new node
    //
    if (tail == NULL)
    {
        // list is empty, so have head point to it
        //
        list->first = node;
        list->size = 1;
    }
    else
    {
        // append to end
        //
        tail->next = node;
        list->size++;
    }

    return true;
}

/*
 * prepend 'item' to the beginning of the LinkedList
 * (ex: add to the front)
 *
 * @param list - the LinkedList to alter
 * @param item - the item to add
 * @return TRUE on success
 */
bool linkedListPrepend(icLinkedList *list, void *item)
{
    if (list == NULL || item == NULL)
    {
        return false;
    }

    // create the node and assign 'item' to it
    //
    listNode *node = createListNode();
    node->item = item;

    // add current 'first' as 'next' of this new node
    //
    node->next = list->first;

    // adjust old head
    //
    list->first = node;

    // update counter
    //
    list->size++;

    return true;
}

/*
 * iterate through the LinkedList to find a particular item
 * for each node in the list, will call the 'searchFunc'
 * with the current item and the 'searchVal' to perform
 * the comparison.
 *
 * @param list - the LinkedList to search through
 * @param searchVal - the value looking for
 * @param searchFunc - the compare function to call for each item
 * @return the 'item' found, or NULL if not located
 */
void *linkedListFind(icLinkedList *list, void *searchVal, linkedListCompareFunc searchFunc)
{
    if (list == NULL || searchVal == NULL || searchFunc == NULL)
    {
        return NULL;
    }

    // iterate through the list, calling searchFunc for each
    //
    listNode *ptr = list->first;
    while (ptr != NULL)
    {
        // notify search function
        //
        if (searchFunc(searchVal, ptr->item) == true)
        {
            // found what we wanted
            //
            return ptr->item;
        }

        // go to the next node
        //
        ptr = ptr->next;
    }

    // not found
    //
    return NULL;
}

void *linkedListRemove(icLinkedList* list, uint32_t offset)
{
    void* item = NULL;

    if ((list != NULL) && (list->size > offset)) {
        listNode *node, *previous;
        uint32_t index;

        /* Loop through all entries.
         * We will either break the loop because we hit the offset
         * or we will bail because we hit the end of the list.
         */
        for (node = list->first, index = 0, previous = NULL;
             (index != offset) && (node != NULL);
             previous = node, node = node->next, index++);

        if (node != NULL) {
            listNode* next = node->next;

            if (previous) {
                previous->next = next;
            } else {
                list->first = next;
            }

            item = node->item;

            if (list->size > 0) list->size--;

            free(node);
        }
    }

    return item;
}

/*
 * iterate through the LinkedList to find a particular item, and
 * if located delete the item and the node.  Operates similar to
 * linkedListFind.
 *
 * @param list - the LinkedList to search through
 * @param searchVal - the value looking for
 * @param searchFunc - the compare function to call for each item
 * @param freeFunc - optional, only needed if the item needs a custom mechanism to release the memory
 * @return TRUE on success
 */
bool linkedListDelete(icLinkedList *list, void *searchVal, linkedListCompareFunc searchFunc, linkedListItemFreeFunc freeFunc)
{
    if (list == NULL || searchVal == NULL || searchFunc == NULL)
    {
        return false;
    }

    // similar to 'find', iterate through until we find a match
    // unfortunately this can't just use the search because we have
    // to know the previous node to preserve our pointers
    //
    listNode *ptr = list->first;
    listNode *last = NULL;
    while (ptr != NULL)
    {
        // see if this matches what they are looking for
        //
        if (searchFunc(searchVal, ptr->item) == true)
        {
            // found match, remove this node
            //
            listNode *forward = ptr->next;
            if (last == NULL)
            {
                // removing first
                //
                list->first = forward;
            }
            else
            {
                last->next = forward;
            }

            // see if custom free function
            //
            if (freeFunc)
            {
                // use custom function
                freeFunc(ptr->item);
            }
            else
            {
                // normal mem cleanup
                free(ptr->item);
            }

            // adjust count
            //
            if (list->size > 0)
            {
                list->size--;
            }

            // free the node
            //
            free(ptr);
            return true;
        }

        // get the next node
        //
        last = ptr;
        ptr = ptr->next;
    }

    return false;
}

/*
 * iterate through the LinkedList, calling 'linkedListIterateFunc' for each
 * item in the list.
 *
 * @param list - the LinkedList to search through
 * @param callback - the function to call for each item
 * @param arg - optional parameter supplied to the 'callback' function
 */
void linkedListIterate(icLinkedList *list, linkedListIterateFunc callback, void *arg)
{
    if (list == NULL || list->size == 0)
    {
        // nothing to loop through
        //
        return;
    }

    // iterate through the list, calling helper for each
    //
    listNode *ptr = list->first;
    while (ptr != NULL)
    {
        // notify callback function
        //
        if (callback(ptr->item, arg) == false)
        {
            // callback told us to stop looping
            //
            break;
        }

        // go to the next node
        //
        ptr = ptr->next;
    }
}

/*
 * return the element at 'offset' (similar to an array).
 * caller should not remove the return object as it is
 * still part of the linkedList.
 *
 * @param list - the LinkedList to search through
 * @param offset - element number, 0 through (getCount - 1)
 */
void *linkedListGetElementAt(icLinkedList *list, uint32_t offset)
{
    if (list == NULL || list->size == 0 || offset >= list->size)
    {
        // nothing to loop through or offset too big
        //
        return NULL;
    }

    // iterate through the list, calling helper for each
    //
    uint32_t count = 0;
    listNode *ptr = list->first;
    while (ptr != NULL)
    {
        // return if we're at the correct place
        //
        if (count == offset)
        {
            return ptr->item;
        }

        // go to the next element
        //
        count++;
        ptr = ptr->next;
    }

    // hit end-of-list before reacing offset
    //
    return NULL;
}

/*
 * clear and destroy the items in the list.  note that
 * each 'item' will just be release via free() unless
 * a 'helper' is provided.
 * NOTE: items in the list are NOT RELEASED if this list is a 'clone' (regardless of 'helper' parameter)
 *
 * @param list - the LinkedList to clear
 * @param helper - optional, only needed if the items need a custom mechanism to release the memory
 *
 * @see linkedListClone()
 */
void linkedListClear(icLinkedList *list, linkedListItemFreeFunc helper)
{
    if (list == NULL || list->size == 0)
    {
        return;
    }

    // iterate through the list, calling helper for each
    //
    listNode *ptr = list->first;
    while (ptr != NULL)
    {
        // free the element within the 'node' object
        // (note we skip if this is a cloned list)
        //
        if (list->cloned == false)
        {
            if (helper)
            {
                // use supplied callback function to clear mem
                helper(ptr->item);
            }
            else if (ptr->item != NULL)
            {
                // normal mem cleanup
                free(ptr->item);
            }
        }

        // get the next node before releasing this one
        //
        listNode *tmp = ptr->next;
        free(ptr);
        ptr = tmp;
    }

    // reset internals
    //
    list->cloned = false;
    list->first = NULL;
    list->size = 0;
}

/*-------------------------------*
 *
 *  icLinkedListIterator
 *
 *-------------------------------*/

/*
 * alternative mechanism to iterating the linked list items
 * should be called initially to create the LinkedListIterator,
 * then followed with calls to linkedListIteratorHasNext and
 * linkedListIteratorGetNext.  once the iteration is done, this
 * MUST BE FREED via linkedListIteratorDestroy
 */
icLinkedListIterator * linkedListIteratorCreate(icLinkedList *list)
{
    if (list == NULL || list->size == 0)
    {
        // nothing to loop through
        //
        return NULL;
    }

    // allocate a listIterator and setup pointers at 'list'
    //
    icLinkedListIterator *retVal = (icLinkedListIterator *)malloc(sizeof(icLinkedListIterator));
    retVal->head = list;
    retVal->curr = retVal->head->first;
    retVal->prev = NULL;

    return retVal;
}

/*
 * free a LinkedListIterator
 */
void linkedListIteratorDestroy(icLinkedListIterator *iterator)
{
    if (iterator != NULL)
    {
        iterator->head = NULL;
        iterator->curr = NULL;
        iterator->prev = NULL;
        free(iterator);
    }
}

/*
 * return if there are more items in the iterator to examine
 */
bool linkedListIteratorHasNext(icLinkedListIterator *iterator)
{
    // look at the 'curr' node in 'iterator' to see if we can move forward or not
    //
    if (iterator != NULL && iterator->curr != NULL)
    {
        // have at least one more node in the list relative to 'curr'
        //
        return true;
    }
    return false;
}

/*
 * internal function to remove the 'prev' pointer.  called by hashMap
 * when deleting from within the iterator directly.  prevents having
 * pointers to bad memory during iteration.
 */
void linkedListIteratorClearPrev(icLinkedListIterator *iterator)
{
    if (iterator != NULL)
    {
        iterator->prev = NULL;
    }
}

/*
 * return the next item available in the LinkedList via the iterator
 */
void *linkedListIteratorGetNext(icLinkedListIterator *iterator)
{
    void *retVal = NULL;

    if (iterator != NULL)
    {
        // get the 'item' pointed to by 'curr'
        //
        if (iterator->curr != NULL)
        {
            // grab pointer to the item (to return)
            //
            retVal = iterator->curr->item;

            // move pointer to 'next' for the subsequent hasNext() call
            //
            iterator->prev = iterator->curr;
            iterator->curr = iterator->curr->next;
        }
        else
        {
            // at the end of the list.  reset pointers
            // to prevent moving past the end
            //
            iterator->prev = NULL;
            iterator->curr = NULL;
        }
    }

    return retVal;
}

/*
 * implementation of 'linkedListCompareFunc' that compares
 * node->item to 'searchVal'.  called by the 'iterator delete'
 * to remove the 'prev' item
 */
static bool iteratorFindByPtr(void *searchVal, void *item)
{
    // searchVal should be a pointer to 'iterator->prev'
    // item should be a point to node->item
    //
    if (searchVal == item)
    {
        return true;
    }
    return false;
}

/*
 * deletes the current item (i.e. the item returned from the last call to
 * linkedListIteratorGetNext) from the underlying LinkedList.
 * just like other 'delete' functions, will release the memory
 * via free() unless a helper is supplied.
 *
 * only valid after 'getNext' is called, and returns if the delete
 * was successful or not.
 */
bool linkedListIteratorDeleteCurrent(icLinkedListIterator *iterator, linkedListItemFreeFunc helper)
{
    // only valid if the iterator 'getNext' has been called
    //
    if (iterator != NULL && iterator->prev != NULL)
    {
        // do the delete by finding the item in the LinkedList
        // that has the same pointer as 'prev'
        //
        bool retVal = linkedListDelete(iterator->head, iterator->prev->item, iteratorFindByPtr, helper);

        // NULL out prev so this cannot be called again until
        // a subsequent call to getNext is made (even if delete failed)
        //
        iterator->prev = NULL;
        return retVal;
    }

    return false;
}

/*
 * simple linked list compare function that can be used to compare strings in a list.
 *
 * @see linkedListFind
 * @see linkedListDelete
 */
bool linkedListStringCompareFunc(void *searchVal, void *item)
{
    if (searchVal != NULL && item != NULL && strcmp(searchVal, item) == 0)
    {
        return true;
    }

    return false;
}

/*
 * simple linked list clone item function that can be used to deep clone lists of strings.
 *
 * @see linkedListDeepClone
 */
void *linkedListCloneStringItemFunc(void *item, void *context)
{
    (void)context; //unused
    return(strdup((char*)item));
}

/*
 * internal helper to allocate mem (and clear)
 * used by other icTypes objects (queue, sorted list, etc)
 */
listNode *createListNode()
{
    listNode *node = (listNode *)malloc(sizeof(listNode));
    memset(node, 0, sizeof(listNode));
    return node;
}

