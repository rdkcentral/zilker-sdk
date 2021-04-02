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

//
// Created by Boyd, Weston on 4/27/18.
//

#include <string.h>

#include "icrule_constraint.h"
#include "icrule_internal.h"

#define ELEMENT_CONSTRAINT_AND "and-expression"
#define ELEMENT_CONSTRAINT_OR "or-expression"
#define ELEMENT_CONSTRAINT_TIME "timeConstraint"

static int parse_time_constraint(xmlNodePtr parent, icLinkedList* time_constraints)
{
    xmlNodePtr node;
    icrule_constraint_time_t* constraint_time;

    constraint_time = malloc(sizeof(icrule_constraint_time_t));
    if (constraint_time == NULL) return -1;

    // Default init the time constraint
    memset(constraint_time, 0, sizeof(icrule_constraint_time_t));

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, "start") == 0) {
                if (icrule_parse_time_slot(node, &constraint_time->start) < 0) {
                    free(constraint_time);
                    return -1;
                }
            } else if (strcmp((const char*) node->name, "end") == 0) {
                if (icrule_parse_time_slot(node, &constraint_time->end) < 0) {
                    free(constraint_time);
                    return -1;
                }
            }
        }
    }

    // Malformed XML as start, and end, are required!
    if ((constraint_time->start.day_of_week == ICRULE_TIME_INVALID) ||
        (constraint_time->end.day_of_week == ICRULE_TIME_INVALID)) {
        free(constraint_time);
        return -1;
    }

    linkedListAppend(time_constraints, constraint_time);

    return 0;
}

void icrule_free_constraint(void* alloc)
{
    if (alloc) {
        icrule_constraint_t* constraint = alloc;

        linkedListDestroy(constraint->child_constraints, icrule_free_constraint);
        linkedListDestroy(constraint->time_constraints, NULL);

        free(constraint);
    }
}

int icrule_parse_constraint(xmlNodePtr parent, icLinkedList* constraints, icrule_constraint_logic_t logic)
{
    xmlNodePtr node;
    icrule_constraint_t* constraint;

    constraint = malloc(sizeof(icrule_constraint_t));
    if (constraint == NULL) {
        return -1;
    }

    memset(constraint, 0, sizeof(icrule_constraint_t));

    constraint->logic = logic;
    constraint->child_constraints = linkedListCreate();
    constraint->time_constraints = linkedListCreate();

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, ELEMENT_CONSTRAINT_AND) == 0) {
                if (icrule_parse_constraint(node, constraint->child_constraints, CONSTRAINT_LOGIC_AND) < 0) {
                    icrule_free_constraint(constraint);
                    return -1;
                }
            } else if (strcmp((const char*) node->name, ELEMENT_CONSTRAINT_OR) == 0) {
                if (icrule_parse_constraint(node, constraint->child_constraints, CONSTRAINT_LOGIC_OR) < 0) {
                    icrule_free_constraint(constraint);
                    return -1;
                }
            } else if (strcmp((const char*) node->name, ELEMENT_CONSTRAINT_TIME) == 0) {
                if (parse_time_constraint(node, constraint->time_constraints) < 0) {
                    icrule_free_constraint(constraint);
                    return -1;
                }
            }
        }
    }

    if ((linkedListCount(constraint->child_constraints) == 0) &&
        (linkedListCount(constraint->time_constraints) == 0)) {
        icrule_free_constraint(constraint);
        return -1;
    }

    linkedListAppend(constraints, constraint);

    return 0;
}
