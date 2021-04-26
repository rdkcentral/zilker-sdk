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
 * icLinkedList.h
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

#ifndef IC_LINKEDLIST_H
#define IC_LINKEDLIST_H

#include <stdint.h>
#include <stdbool.h>
#include "icLinkedListFuncs.h"
#include "sbrm.h"

/*-------------------------------*
 *
 *  icLinkedList
 *
 *-------------------------------*/

/*
 * the LinkedList object representation.
 * allows the caller to switch out head/tail and have
 * multiple instances without concern of memory reallocation
 */
typedef struct _icLinkedList icLinkedList;

/*
 * create a new linked list.  will need to be released when complete
 *
 * @return a new LinkedList object
 * @see linkedListDestroy()
 */
icLinkedList *linkedListCreate();

/*
 * create a shallow-clone of an existing linked list.
 * will need to be released when complete, HOWEVER,
 * the items in the cloned list are not owned by it
 * and therefore should NOT be released.
 *
 * @see linkedListDestroy()
 * @see linkedListIsClone()
 */
icLinkedList *linkedListClone(icLinkedList *src);

/*
 * create a deep-clone of an existing linked list.
 * will need to be released when complete.
 *
 * @see linkedListDestroy()
 */
icLinkedList *linkedListDeepClone(icLinkedList *src, linkedListCloneItemFunc cloneItemFunc, void *context);

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
void linkedListDestroy(icLinkedList *list, linkedListItemFreeFunc helper);

/**
 * Auto-free a linked list whose keys/values do not need a linkedListItemFreeFunc
 * @param list
 */
inline void linkedListDestroy_generic__auto(icLinkedList **list)
{
    linkedListDestroy(*list, NULL);
}

/*
 * return true if this list was created via linkedListClone
 *
 * @see linkedListClone
 */
bool linkedListIsClone(icLinkedList *list);

/*
 * return the number of elements in the list
 *
 * @param list - the LinkedList to count
 */
uint16_t linkedListCount(icLinkedList *list);

/*
 * append 'item' to the end of the LinkedList
 * (ex: add to the end)
 *
 * @param list - the LinkedList to alter
 * @param item - the item to add
 * @return TRUE on success
 */
bool linkedListAppend(icLinkedList *list, void *item);

/*
 * prepend 'item' to the beginning of the LinkedList
 * (ex: add to the front)
 *
 * @param list - the LinkedList to alter
 * @param item - the item to add
 * @return TRUE on success
 */
bool linkedListPrepend(icLinkedList *list, void *item);

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
void *linkedListFind(icLinkedList *list, void *searchVal, linkedListCompareFunc searchFunc);

/*
 * remove the element at 'offset' (similar to an array).
 *
 * @param list - the LinkedList to search through
 * @param offset - element number, 0 through (getCount - 1)
 * @return the removed element or NULL if invalid list/offset given
 */
void *linkedListRemove(icLinkedList* list, uint32_t offset);

/*
 * iterate through the LinkedList to find a particular item, and
 * if located delete the item and the node.  Operates similar to
 * findInLinkedList.
 *
 * @param list - the LinkedList to search through
 * @param searchVal - the value looking for
 * @param searchFunc - the compare function to call for each item
 * @param freeFunc - optional, only needed if the item needs a custom mechanism to release the memory
 * @return TRUE on success
 */
bool linkedListDelete(icLinkedList *list, void *searchVal, linkedListCompareFunc searchFunc, linkedListItemFreeFunc freeFunc);

/*
 * iterate through the LinkedList, calling 'linkedListIterateFunc' for each
 * item in the list.
 *
 * @param list - the LinkedList to search through
 * @param callback - the function to call for each item
 * @param arg - optional parameter supplied to the 'callback' function 
 */
void linkedListIterate(icLinkedList *list, linkedListIterateFunc callback, void *arg);

/*
 * return the element at 'offset' (similar to an array).
 * caller should not remove the return object as it is
 * still part of the linkedList.
 *
 * @param list - the LinkedList to search through
 * @param offset - element number, 0 through (getCount - 1)
 */
void *linkedListGetElementAt(icLinkedList *list, uint32_t offset);

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
void linkedListClear(icLinkedList *list, linkedListItemFreeFunc helper);

/*-------------------------------*
 *
 *  icLinkedListIterator
 *
 *-------------------------------*/

typedef struct _icLinkedListIterator icLinkedListIterator;

/**
 * Convenience macro to create a scope bound linkedListIterator. When it goes out of scope,
 * linkedListIteratorDestroy will be called automatically.
 */
#define sbIcLinkedListIterator AUTO_CLEAN(linkedListIteratorDestroy__auto) icLinkedListIterator

/*
 * alternative mechanism to iterating the linked list items
 * should be called initially to create the LinkedListIterator,
 * then followed with calls to linkedListIteratorHasNext and
 * linkedListIteratorGetNext.  once the iteration is done, this
 * MUST BE FREED via linkedListIteratorDestroy
 *
 * @param list - the LinkedList to iterate
 * @see linkedListIteratorDestroy()
 */
icLinkedListIterator *linkedListIteratorCreate(icLinkedList *list);

/*
 * free a LinkedListIterator
 */
void linkedListIteratorDestroy(icLinkedListIterator *iterator);

inline void linkedListIteratorDestroy__auto(icLinkedListIterator **iter)
{
    linkedListIteratorDestroy(*iter);
}

/*
 * return if there are more items in the iterator to examine
 */
bool linkedListIteratorHasNext(icLinkedListIterator *iterator);

/*
 * return the next item available in the LinkedList via the iterator
 */
void *linkedListIteratorGetNext(icLinkedListIterator *iterator);

/*
 * deletes the current item (i.e. the item returned from the last call to
 * linkedListIteratorGetNext) from the underlying LinkedList.
 * just like other 'delete' functions, will release the memory
 * via free() unless a helper is supplied.
 *
 * only valid after 'getNext' is called, and returns if the delete
 * was successful or not.
 */
bool linkedListIteratorDeleteCurrent(icLinkedListIterator *iterator, linkedListItemFreeFunc helper);


#endif // IC_LINKEDLIST_H
