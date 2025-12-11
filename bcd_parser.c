#include "bcd_parser.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * Minimal mapping from regf hive to BCD_STORE.
 * Assumptions (documented for clarity):
 * - The hive root key represents the BCD store root.
 * - Each immediate subkey under the root is a BCD object. The key name is a GUID string.
 * - Values under each object represent BCD elements. The value name is interpreted as a
 *   hexadecimal element type identifier (e.g., "11000001").
 * - Value data kind is mapped from registry type: REG_SZ -> string, REG_DWORD -> integer,
 *   REG_BINARY -> binary. Unknown types are marked as BCD_ELEMENT_UNKNOWN.
 * These rules are sufficient for enumerating objects and elements for this exercise.
 */

#define REG_TYPE_NONE 0
#define REG_TYPE_SZ 1
#define REG_TYPE_EXPAND_SZ 2
#define REG_TYPE_BINARY 3
#define REG_TYPE_DWORD 4
#define REG_TYPE_MULTI_SZ 7
#define REG_TYPE_QWORD 11

static uint32_t parse_hex_to_uint32(const char *text, size_t len, int *ok)
{
    uint32_t value = 0;
    *ok = 0;
    for (size_t i = 0; i < len; ++i) {
        char c = text[i];
        int digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = 10 + (c - 'a');
        else if (c >= 'A' && c <= 'F') digit = 10 + (c - 'A');
        else return 0;
        value = (value << 4) | (uint32_t)digit;
    }
    *ok = 1;
    return value;
}

static void fill_object_defaults(BCD_OBJECT *obj)
{
    memset(obj, 0, sizeof(*obj));
}

int BcdStoreLoadFromHive(BCD_STORE *store, REGF_HIVE *hive)
{
    if (!store || !hive) return BCD_ERR_INVALID_ARG;
    BcdStoreReset(store);
    REGF_KEY *root = RegfGetRootKey(hive);
    if (!root) return BCD_ERR_PARSE;

    int objectCount = RegfGetSubKeyCount(root);
    for (int i = 0; i < objectCount; ++i) {
        REGF_KEY *objKey = RegfGetSubKeyAt(root, i);
        if (!objKey) continue;
        BCD_OBJECT obj;
        fill_object_defaults(&obj);
        if (BcdParseObjectId(RegfGetKeyName(objKey), &obj.id) != BCD_OK) {
            RegfReleaseKey(objKey);
            continue;
        }
        obj.objectType = 0;

        int valCount = RegfGetValueCount(objKey);
        for (int v = 0; v < valCount; ++v) {
            REGF_VALUE *val = RegfGetValueAt(objKey, v);
            if (!val) continue;
            BCD_ELEMENT element;
            memset(&element, 0, sizeof(element));
            const char *name = RegfGetValueName(val);
            int ok = 0;
            element.type = parse_hex_to_uint32(name, strlen(name), &ok);
            if (!ok) {
                RegfReleaseValue(val);
                continue;
            }
            uint32_t regType = RegfGetValueType(val);
            size_t dataSize = 0;
            const void *data = RegfGetValueData(val, &dataSize);
            if (!data) {
                element.kind = BCD_ELEMENT_UNKNOWN;
            } else if (regType == REG_TYPE_SZ || regType == REG_TYPE_EXPAND_SZ || regType == REG_TYPE_MULTI_SZ) {
                element.kind = BCD_ELEMENT_STRING;
                size_t copyLen = dataSize < (BCD_MAX_STRING_LEN - 1) ? dataSize : (BCD_MAX_STRING_LEN - 1);
                memcpy(element.data.stringValue, data, copyLen);
                element.data.stringValue[copyLen] = '\0';
            } else if (regType == REG_TYPE_DWORD) {
                element.kind = BCD_ELEMENT_INTEGER;
                element.data.integerValue = (uint64_t)RegfGetValueDataAsUint32(val, &ok);
                if (!ok) element.kind = BCD_ELEMENT_UNKNOWN;
            } else if (regType == REG_TYPE_QWORD && dataSize >= 8) {
                element.kind = BCD_ELEMENT_INTEGER;
                const unsigned char *p = (const unsigned char *)data;
                element.data.integerValue = (uint64_t)p[0] | ((uint64_t)p[1] << 8) |
                                            ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 24) |
                                            ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40) |
                                            ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
            } else if (regType == REG_TYPE_BINARY) {
                element.kind = BCD_ELEMENT_BINARY;
                size_t copyLen = dataSize < BCD_MAX_BINARY_SIZE ? dataSize : BCD_MAX_BINARY_SIZE;
                memcpy(element.data.binaryValue.data, data, copyLen);
                element.data.binaryValue.size = copyLen;
            } else {
                element.kind = BCD_ELEMENT_UNKNOWN;
            }
            if (BcdObjectAddElement(&obj, &element) != BCD_OK) {
                RegfReleaseValue(val);
                break;
            }
            RegfReleaseValue(val);
        }
        if (BcdStoreAddObject(store, &obj) != BCD_OK) {
            RegfReleaseKey(objKey);
            return BCD_ERR_CAPACITY;
        }
        RegfReleaseKey(objKey);
    }
    return BCD_OK;
}

