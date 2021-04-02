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
// Created by Boyd, Weston on 5/4/18.
//

#include <icrule/icrule.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "sheens_json.h"
#include "js/timeFunctions.js.h"

// TODO: This is NOT really RPN. But instead top-down queueing. Need to see if using real RPN will help

static const char* or_str  = "|";
static const char* and_str = "&";
static const char* ret_str = "~";
static const char* allowed_or_str = sheens_allowed_key " = " sheens_allowed_key " ||";
static const char* allowed_and_str = sheens_allowed_key " = " sheens_allowed_key " &&";

/** Free function names that were placed on the RPN stack.
 *
 * Note: The bitwise (and return) operators will not be
 * freed.
 *
 * @param alloc The function name that will have its memory
 * released.
 */
static void free_constraint_rpn_js(void* alloc)
{
    char* operator = alloc;

    if (operator) {
        switch (operator[0]) {
            case '|':
            case '&':
            case '~':
                break;
            default:
                free(operator);
                break;
        }
    }
}

/** Un-roll the RPN stack and generate the JavaScript logic expression.
 *
 * This is a complex routine! The RPN is recursively parsed by grouping
 * each bitwise logic with a final "return", or "enter".
 *
 * @param rpn The RPN stack.
 * @param js The starting point to write auto-generated JavaScript into.
 * @param logic The previous constraint logic expression. This is so
 * that we can properly group the different expressions.
 * @return The total number of bytes written into the JavaScript buffer. -1 on
 * failure, and errno will be set.
 */
static int build_constraint_allowed_js(icLinkedList* rpn, char* js, char logic)
{
    int bytes_written = 0;
    const char* func_logic_str = NULL;
    const char* logic_str;

    /* Determine the logic string that will
     * be used in the JavaScript generation.
     * One the first pass in there will not be
     * a logic expression so we can properly
     * group the expression blocks.
     */
    switch (logic) {
        case '|':
            logic_str = " || ";
            break;
        case '&':
            logic_str = " && ";
            break;
        default:
            logic_str = "";
            break;
    }

    while (linkedListCount(rpn) > 0) {
        char *entry = linkedListRemove(rpn, 0);

        if (entry) {
            switch (entry[0]) {
                case '|':
                case '&':
                    /* A new expression grouping so place in the opening
                     * parentheses. Then call down into the next
                     * RPN expression.
                     */
                    bytes_written += sprintf(js + bytes_written, "(");
                    bytes_written += build_constraint_allowed_js(rpn, js + bytes_written, entry[0]);
                    break;
                case '~': {
                    char* peek = linkedListGetElementAt(rpn, 0);

                    bytes_written += sprintf(js + bytes_written, ")");

                    /* If the next expression in the RPN stack is a 'return' then
                     * we do _not_ want to inject the JS logic, but instead want
                     * to just of the close parentheses. If the next expression
                     * is anything else then add in the JS logic.
                     */
                    if (peek && (peek[0] != '~')) {
                        bytes_written += sprintf(js + bytes_written, "%s", logic_str);
                    }

                    return bytes_written;
                }
                default:
                    /* For the first entry in the JS expression (after an opening
                     * parentheses) we do _not_ want a JS logic expression.
                     */
                    if (func_logic_str) {
                        bytes_written += sprintf(js + bytes_written, "%s%s(_.bindings)", logic_str, entry);
                    } else {
                        bytes_written += sprintf(js + bytes_written, "%s(_.bindings)", entry);
                        func_logic_str = logic_str;
                    }

                    break;
            }

            /* Because we removed the entry from the list we must free it
             * ourselves, however we need to take care that it is an
             * actual function expression and not a logic expression.
             * Thus call our custom free routine.
             */
            free_constraint_rpn_js(entry);
        }
    }

    return bytes_written;
}

static int internal_sheens_constraints2javascript(icLinkedListIterator* iterator, char* js, icLinkedList* rpn, bool *timeFuncsAdded)
{
    icLinkedList* __rpn = rpn;
    int bytes_written = 0;

    /* Preserve RPN so that we can determine if this was
     * the beginning of the recursive routine later.
     */
    if (__rpn == NULL) {
        __rpn = linkedListCreate();
    }

    while (linkedListIteratorHasNext(iterator)) {
        icrule_constraint_t* constraint = linkedListIteratorGetNext(iterator);

        if (constraint) {
            int ret;
            char* func_name;
            bool first_allowed = true;
            const char* allow_check_reserve;

            /* Push the constraints logic into the RPN stack. */
            linkedListAppend(__rpn,
                             (void*) ((constraint->logic == CONSTRAINT_LOGIC_OR) ? or_str : and_str));

            ret = internal_sheens_constraints2javascript(linkedListIteratorCreate(constraint->child_constraints),
                                                js + bytes_written,
                                                __rpn,
                                                timeFuncsAdded);
            if (ret < 0) {
                linkedListIteratorDestroy(iterator);

                if (rpn == NULL) {
                    linkedListDestroy(__rpn, free_constraint_rpn_js);
                }

                return -1;
            }

            bytes_written += ret;

            /* The function name will always be the same so we
             * can hard-code the number of bytes allocated. Currently
             * we are using the memory address of the constraint
             * to make the function name unique. This uniqueness
             * is only necessary for a single Sheens scheme, and not
             * across _all_ schemas.
             *
             * The algorithm to determine the function name size is:
             * size = strlen("isAllowed_0123456701234567") + 1
             *
             * The address may be 64-bit and the +1 is for the
             * null terminator.
             */
            func_name = malloc(28);
            if (func_name == NULL) {
                linkedListIteratorDestroy(iterator);

                if (rpn == NULL) {
                    linkedListDestroy(__rpn, free_constraint_rpn_js);
                }

                errno = ENOMEM;
                return -1;
            }

            sprintf(func_name, "isAllowed_%016lx", (unsigned long) constraint);

            /* We need to translate the logic flag into actual
             * JS strings so we can generate the JS properly.
             * Having it here allows us to perform that task
             * once per constraint.
             */
            switch (constraint->logic) {
                case CONSTRAINT_LOGIC_OR:
                    allow_check_reserve = allowed_or_str;
                    break;
                case CONSTRAINT_LOGIC_AND:
                default:
                    allow_check_reserve = allowed_and_str;
                    break;
            }

            // Check if we need to include our time functions
            if (*timeFuncsAdded == false && linkedListCount(constraint->time_constraints) > 0) {
                bytes_written += sprintf(js + bytes_written, "%s", TIMEFUNCTIONS_JS_BLOB);
                *timeFuncsAdded = true;
            }

            /* Create the JS function that will represent this constraint. This allows
             * us to bundle one constraint into a nice little package that we can then
             * check the status of once we un-roll all of the constraints.
             */
            bytes_written += sprintf(js + bytes_written, "function %s(bindings) {\n", func_name);
            bytes_written += sprintf(js + bytes_written, "  var " sheens_allowed_key ";\n");

            if (linkedListCount(constraint->time_constraints) > 0) {
                icLinkedListIterator* timeiter = linkedListIteratorCreate(constraint->time_constraints);

                /* Pull the 'now' time from the JS engine. This way we can verify the
                 * time constraints.
                 */
                bytes_written += sprintf(js + bytes_written,
                                         "  var dateNow = new Date();\n"
                                         "  var nowSeconds = (dateNow.getHours() * 60 * 60) + (dateNow.getMinutes() * 60);\n"
                                         "  var daysOfWeek = (1 << dateNow.getDay());\n");

                while (linkedListIteratorHasNext(timeiter)) {
                    icrule_constraint_time_t* value = linkedListIteratorGetNext(timeiter);

                    if (value) {
                        const char* allowed_check;

                        if (first_allowed) {
                            allowed_check = sheens_allowed_key " =";
                            first_allowed = false;
                        } else {
                            allowed_check = allow_check_reserve;
                        }

                        /* Make sure the day of the week is in our constraints. */
                        bytes_written += sprintf(js + bytes_written,
                                                 "%s (((%u & daysOfWeek) != 0) && ",
                                                 allowed_check,
                                                 value->start.day_of_week);

                        if (value->start.use_exact_time) {
                            /* If the 'end' time is _before_ the 'start' time then
                             * we have a roll-over of the 24hr period. Thus we
                             * need to handle this condition specially.
                             */
                            if (value->end.time.seconds < value->start.time.seconds) {
                                bytes_written += sprintf(js + bytes_written,
                                                         "((nowSeconds >= %u) || (nowSeconds <= %u)));\n",
                                                         value->start.time.seconds,
                                                         value->end.time.seconds);
                            } else {
                                bytes_written += sprintf(js + bytes_written,
                                                         "((nowSeconds >= %u) && (nowSeconds <= %u)));\n",
                                                         value->start.time.seconds,
                                                         value->end.time.seconds);
                            }
                        } else {
                            /* It would seem (by observation and NOT by syntax of the XSD) that
                             * we cannot have a rule that is "start: sunrise" and "end: sunrise", or
                             * the same for sunset. Thus we really don't care what "end" will be as
                             * we can safely (we believe) assume that "end" is always the opposite
                             * of "start".
                             */
                            switch (value->start.time.sun_time) {
                                case ICRULE_SUNTIME_SUNRISE:
                                    bytes_written += sprintf(js + bytes_written,
                                                             "((nowSeconds >= new WeekTime(bindings['" sheens_sunrise_bound_key "']).seconds) && "
                                                             "(nowSeconds <= new WeekTime(bindings['" sheens_sunset_bound_key "']).seconds)));\n");
                                    break;
                                case ICRULE_SUNTIME_SUNSET:
                                    bytes_written += sprintf(js + bytes_written,
                                                             "((nowSeconds >= new WeekTime(bindings['" sheens_sunset_bound_key "']).seconds) || "
                                                             "(nowSeconds <= new WeekTime(bindings['" sheens_sunrise_bound_key "']).seconds)));\n");
                                    break;
                            }
                        }
                    }
                }

                linkedListIteratorDestroy(timeiter);
            }

            if (first_allowed) {
                bytes_written += sprintf(js + bytes_written, "  " sheens_allowed_key " = true;\n");
            }

            bytes_written += sprintf(js + bytes_written, "  return " sheens_allowed_key ";\n}\n");

            /* Push the new function name into the RPN stack */
            linkedListAppend(__rpn, func_name);
            linkedListAppend(__rpn, (void*) ret_str);
        }
    }

    /* All of the iterators are created inline with the function.
     * Thus we go ahead and destroy the iterator before returning.
     */
    linkedListIteratorDestroy(iterator);

    if (rpn == NULL) {
        /* This is the top most constraint in the RPN stack. Now
         * we need to set up our 'final allowed' check and then
         * un-roll the RPN stack into JS.
         */
        bytes_written += sprintf(js + bytes_written, "var final_allowed;\n");
        bytes_written += sprintf(js + bytes_written, "final_allowed = ");

        /* Recursively un-roll the RPN stack into JS. */
        bytes_written += build_constraint_allowed_js(__rpn, js + bytes_written, -1);

        /* We didn't let the un-roll process close down the Boolean expression.
         * This is because we don't know "when" it will end in the recursion stack
         * _easily_. It is thus easier to just close it down outside the un-roll
         * process.
         */
        bytes_written += sprintf(js + bytes_written, ";\n");
        bytes_written += sprintf(js + bytes_written, "_.bindings['" sheens_allowed_key "'] = final_allowed;\n");
        bytes_written += sprintf(js + bytes_written, "return _.bindings;\n");

        /* Finish of that darn RPN stack! */
        linkedListDestroy(__rpn, free_constraint_rpn_js);
    }

    return bytes_written;
}

/** Generate the JavaScript that will handle all of the constraint checking
 * for the automation.
 *
 * This is a complicated routine! First, it recursively dives through each
 * constraint into it finds the final 'child' for a given constraint tree. Second,
 * it builds individual JavaScript _functions_ for each constraint. This produces
 * a nice little package for each constraint so that they may contain their specific
 * OR/AND bitwise logic. Finally, each bitwise OR/AND logic, and new function name, is
 * placed into an RPN stack. The RPN stack is used to build the final constraint
 * verification check while maintaining proper 'order of operation' for the
 * constraints.
 *
 * Example:
 * RPN Stack:
 * &&
 * ||
 * isAllowed_1
 * isAllowed_2
 * ~
 * ||
 * isAllowed_3
 * isAllowed_4
 * ~
 * ~
 *
 * Note:
 * '&&' = bitwise AND
 * '||' = bitwise OR
 * '~'  = return/enter
 *
 * Final verification output:
 * final_allowed = ((isAllowed_1() || isAllowed_2()) && ((isAllowed_3() || isAllowed_4()))
 *
 * @param iterator The iterator that holds all constraints that are to be processed.
 * @param js The starting point for all generated JavaScript for this particular constraint.
 * @param rpn The RPN stack to push logic and auto-generated function names onto.
 * @return The number of bytes written to the JavaScript buffer for this constraint, and
 * all of its children. -1 on failure and the errno will be set.
 */
int sheens_constraints2javascript(icLinkedListIterator* iterator, char* js, icLinkedList* rpn)
{
    bool timeFuncsAdded = false;
    return internal_sheens_constraints2javascript(iterator, js, rpn, &timeFuncsAdded);
}
