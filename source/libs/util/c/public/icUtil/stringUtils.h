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
 * stringUtils.h
 *
 * Utilities for strings
 *
 * Author: jgleason - 9/23/15 (moved tlea's stringReplace function here)
 *-----------------------------------------------*/

#ifndef IC_STRINGUTILS_H
#define IC_STRINGUTILS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * take an original string a replace a substring within it
 * returns a new string with the substring replacement
 *
 * Inputs:
 *   orig - the original string
 *   rep  - the substring being replaced
 *   with - the new substring to replace rep in orig
 * Returns:
 *   new string with substring replaced
 */
char *stringReplace(const char *orig, const char *rep, const char *with);

/*
 * creates a random alpha-numeric token
 *
 * Inputs:
 *   minLength  - the minimum length (in chars) of the token
 *   maxLength  - the maximum length (in chars) of the token
 *   seedAdder  - this number is added to the current time when generating the seed
 *                   (useful if calling multiple times in a row)
 *
 * Returns:
 *   random alpha-numeric string or NULL if error occurs
 *
 * Caller MUST free the allocated token
 */
char *generateRandomToken(uint16_t minLength, uint16_t maxLength, int seedAdder);

/*
 * safely copy a source string into a destination string, preventing
 * overrunning memory of 'dest'.
 * uses the strlen of 'src' and the supplied 'maxDestChars' to copy
 * as many characters as it can without overflowing the buffer.
 *
 * returns false if something went wrong and not able to do the copy.
 */
bool safeStringCopy(char *dest, uint16_t maxDestChars, const char *src);

/*
 * safely appends a source string to the end of a destination string,
 * preventing overrunning memory of 'dest'.
 * uses the strlen of 'src' and the supplied 'maxDestChars' to copy
 * as many characters as it can without overflowing the buffer.
 *
 * returns false if something went wrong and not able to do the append.
 */
bool safeStringAppend(char *dest, uint16_t maxDestChars, const char *src);

/*
 * performs a comparison against the two strings.  can be used
 * for equality or sorting.
 *
 * returns:
 * negative-num - if 'left' is greater,
 * 0            - if they are equal (or both NULL)
 * positive-num - if 'right' is greater.
 */
int8_t stringCompare(const char *left, const char *right, bool ignoreCase);

/*
 * return if the 'string' starts with the same 'prefix'
 */
bool stringStartsWith(const char *string, const char *prefix, bool ignoreCase);

/*
 * return if the 'string' ends with the same 'suffix'
 */
bool stringEndsWith(const char *string, const char *suffix, bool ignoreCase);

/*
 * return a new string with the leading/trailing whitepace removed.
 * caller must release returned memory via free().
 */
char *trimString(const char *src);

/*
 * break a string in two using a token character.
 * ex:  abc.123 --> abc 123
 *
 * the 'leftOut' and 'rightOut' should exist and be
 * large enough to hold the characters.
 *
 * returns false if not able to perform the operation
 */
bool stringSplitOnToken(const char *inputStr, const char token, char *leftOut, char *rightOut);

/*
 * convert the string to all lower case characters.
 */
void stringToLowerCase(char *inputStr);

/*
 * convert the string to all upper case characters.
 */
void stringToUpperCase(char *inputStr);

/*
 * convert the string to camel case and returns the new string
 * Uses chars: '_', '-', ' '
 * To separate the desired words
 * NOTE: caller must free return string
 */
char *stringToCamelCase(const char *inputStr);

/*
 * helper function to safely allocate and create a string
 * using var_args (think of this as a sprintf() that creates mem)
 *
 * NOTE: caller must free the returned string
 */
char *stringBuilder(const char *format, ...);

/**
 * MT-safe strerror (strerror_r adapter for both XSI compliant and _GNU_SOURCE variants)
 * @return a heap-allocated string representing the error (remember to free it)
 */
char *strerrorSafe(int errnum);

/**
 * Edit a string by removing and/or inserting text.
 *
 * @param str The string to edit.
 * @param offset The offset at which characters will be erased and/or inserted.
 * @param removeCount The number of characters to erase after offset. May be 0.
 * @param newText An optional string to insert at offset. If NULL, nothing is inserted.
 * @return A pointer to the edited str (NULL on error).
 * @note str will not be valid after invoking this.
 */
char *stringEdit(char *str, size_t offset, size_t removeCount, const char *newText);

/**
 * Convert a string to a uint8_t using standard base detection conventions
 * @param str string to convert
 * @param num uint8_t to write into
 * @return true if success, false otherwise
 */
bool stringToUint8(const char *str, uint8_t *num);

/**
 * Convert a string to a uint16_t using standard base detection conventions
 * @param str string to convert
 * @param num uint16_t to write into
 * @return true if success, false otherwise
 */
bool stringToUint16(const char *str, uint16_t *num);

/**
 * Convert a string to a uint32_t using standard base detection conventions
 * @param str string to convert
 * @param num uint32_t to write into
 * @return true if success, false otherwise
 */
bool stringToUint32(const char *str, uint32_t *num);

/**
 * Convert a string to a uint64_t using standard base detection conventions
 * @param str string to convert
 * @param num uint64_t to write into
 * @return true if success, false otherwise
 */
bool stringToUint64(const char *str, uint64_t *num);

/**
 * Convert a string to a number in the given range
 * @param str string to convert
 * @param num uint64_t to write into
 * @param base base of the number in the string
 * @param minValue minimum value in range(inclusive)
 * @param maxValue maximum value in range(inclusive)
 * @return true if success and is within range, false otherwise
 */
bool stringToUnsignedNumberWithinRange(const char *str, uint64_t *num, uint8_t base, uint64_t minValue, uint64_t maxValue);

/**
 * Convert a string to a int8_t using standard base detection conventions
 * @param str string to convert
 * @param num int8_t to write into
 * @return true if success, false otherwise
 */
bool stringToInt8(const char *str, int8_t *num);

/**
 * Convert a string to a int16_t using standard base detection conventions
 * @param str string to convert
 * @param num int16_t to write into
 * @return true if success, false otherwise
 */
bool stringToInt16(const char *str, int16_t *num);

/**
 * Convert a string to a int32_t using standard base detection conventions
 * @param str string to convert
 * @param num int32_t to write into
 * @return true if success, false otherwise
 */
bool stringToInt32(const char *str, int32_t *num);

/**
 * Convert a string to a int64_t using standard base detection conventions
 * @param str string to convert
 * @param num int64_t to write into
 * @return true if success, false otherwise
 */
bool stringToInt64(const char *str, int64_t *num);

/**
 * Convert a string to a number in the given range
 * @param str string to convert
 * @param num uint64_t to write into
 * @param base base of the number in the string
 * @param minValue minimum value in range(inclusive)
 * @param maxValue maximum value in range(inclusive)
 * @return true if success and is within range, false otherwise
 */
bool stringToNumberWithinRange(const char *str, int64_t *num, uint8_t base, int64_t minValue, int64_t maxValue);

/**
 * convert a string to a bool; returning whether the string could be converted or not
 * @param str the string to convert
 * @param result the result of the conversion
 * @return true if the string could be converted, false otherwise
 */
bool stringToBoolStrict(const char *str, bool *result);

/**
 * Convert a string to a boolean
 * @param str string to convert
 * @return true if string is true, yes, or 1. False if otherwise
 */
bool stringToBool(const char *str);

/*
 * return the string version of the boolean.  handy for logging
 */
const char *stringValueOfBool(bool flag);

/**
 * Coalesce a string with an alternative
 * @param str The nullable string
 * @param alt The alternative to replace NULL with. If this is also NULL, the replacement will be empty ("").
 * @return
 */
inline const char *stringCoalesceAlt(const char *str, const char *alt)
{
    if (alt == NULL)
    {
        alt = "";
    }

    return str == NULL ? alt : str;
}

/**
 * Coalesce a string constant, converting NULL to "(null)"
 * @param str
 * @return
 */
inline const char *stringCoalesce(const char *str)
{
    return stringCoalesceAlt(str, "(null)");
}

/**
 * Write a bitmap (up to 64 bits) as a string, indicating set bit numbers (1-indexed).
 * @param bitmap A bitmap (8-64 bits)
 * @param mapSize the map size, in bytes
 * @return the string which the caller must free
 */
char *bitmapToStr(uint64_t bitmap, size_t mapSize);

/**
 * Returns true if the given string is NULL or if its first character is a null-terminating character.
 *
 * @param str - The string the check
 * @return
 */
inline bool stringIsEmpty(const char *str)
{
    return (str == NULL || str[0] == '\0');
}

#endif // IC_STRINGUTILS_H
