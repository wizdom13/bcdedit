#include "bcd.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static int parse_hex16(const char *text, uint16_t *out)
{
    uint16_t value = 0;
    for (int i = 0; i < 4; ++i) {
        int d = hex_digit(text[i]);
        if (d < 0) return BCD_ERR_PARSE;
        value = (uint16_t)((value << 4) | (uint16_t)d);
    }
    *out = value;
    return BCD_OK;
}

static int parse_hex32(const char *text, uint32_t *out)
{
    uint32_t value = 0;
    for (int i = 0; i < 8; ++i) {
        int d = hex_digit(text[i]);
        if (d < 0) return BCD_ERR_PARSE;
        value = (value << 4) | (uint32_t)d;
    }
    *out = value;
    return BCD_OK;
}

static int parse_hex8_pair(const char *text, uint8_t *out)
{
    int hi = hex_digit(text[0]);
    int lo = hex_digit(text[1]);
    if (hi < 0 || lo < 0) return BCD_ERR_PARSE;
    *out = (uint8_t)((hi << 4) | lo);
    return BCD_OK;
}

int BcdStoreInit(BCD_STORE *store)
{
    if (!store) return BCD_ERR_INVALID_ARG;
    store->objectCount = 0;
    return BCD_OK;
}

void BcdStoreReset(BCD_STORE *store)
{
    if (!store) return;
    store->objectCount = 0;
}

int BcdStoreAddObject(BCD_STORE *store, const BCD_OBJECT *object)
{
    if (!store || !object) return BCD_ERR_INVALID_ARG;
    if (store->objectCount >= BCD_MAX_OBJECTS) return BCD_ERR_CAPACITY;
    store->objects[store->objectCount] = *object;
    store->objectCount++;
    return BCD_OK;
}

BCD_OBJECT *BcdStoreFindObjectById(BCD_STORE *store, const BCD_OBJECT_ID *id)
{
    if (!store || !id) return NULL;
    for (size_t i = 0; i < store->objectCount; ++i) {
        if (BcdIdsEqual(&store->objects[i].id, id)) {
            return &store->objects[i];
        }
    }
    return NULL;
}

size_t BcdStoreGetObjectCount(const BCD_STORE *store)
{
    return store ? store->objectCount : 0;
}

BCD_OBJECT *BcdStoreGetObjectAt(BCD_STORE *store, size_t index)
{
    if (!store) return NULL;
    if (index >= store->objectCount) return NULL;
    return &store->objects[index];
}

int BcdObjectAddElement(BCD_OBJECT *object, const BCD_ELEMENT *element)
{
    if (!object || !element) return BCD_ERR_INVALID_ARG;
    if (object->elementCount >= BCD_MAX_ELEMENTS_PER_OBJECT) return BCD_ERR_CAPACITY;
    object->elements[object->elementCount] = *element;
    object->elementCount++;
    return BCD_OK;
}

BCD_ELEMENT *BcdObjectFindElement(BCD_OBJECT *object, uint32_t elementType)
{
    if (!object) return NULL;
    for (size_t i = 0; i < object->elementCount; ++i) {
        if (object->elements[i].type == elementType) {
            return &object->elements[i];
        }
    }
    return NULL;
}

int BcdParseObjectId(const char *text, BCD_OBJECT_ID *outId)
{
    if (!text || !outId) return BCD_ERR_INVALID_ARG;
    size_t len = strlen(text);
    if (len != 38 || text[0] != '{' || text[37] != '}') return BCD_ERR_PARSE;
    /* Format: {8-4-4-4-12} */
    if (parse_hex32(text + 1, &outId->data1) != BCD_OK) return BCD_ERR_PARSE;
    if (text[9] != '-') return BCD_ERR_PARSE;
    if (parse_hex16(text + 10, &outId->data2) != BCD_OK) return BCD_ERR_PARSE;
    if (text[14] != '-') return BCD_ERR_PARSE;
    if (parse_hex16(text + 15, &outId->data3) != BCD_OK) return BCD_ERR_PARSE;
    if (text[19] != '-') return BCD_ERR_PARSE;
    if (parse_hex8_pair(text + 20, &outId->data4[0]) != BCD_OK) return BCD_ERR_PARSE;
    if (parse_hex8_pair(text + 22, &outId->data4[1]) != BCD_OK) return BCD_ERR_PARSE;
    if (text[24] != '-') return BCD_ERR_PARSE;
    for (int i = 0; i < 6; ++i) {
        if (parse_hex8_pair(text + 25 + (i * 2), &outId->data4[2 + i]) != BCD_OK) return BCD_ERR_PARSE;
    }
    return BCD_OK;
}

int BcdFormatObjectId(const BCD_OBJECT_ID *id, char *buffer, size_t bufferSize)
{
    if (!id || !buffer || bufferSize < 39) return BCD_ERR_INVALID_ARG;
    int n = snprintf(buffer, bufferSize,
                     "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                     id->data1, id->data2, id->data3,
                     id->data4[0], id->data4[1], id->data4[2], id->data4[3],
                     id->data4[4], id->data4[5], id->data4[6], id->data4[7]);
    if (n < 0 || (size_t)n >= bufferSize) return BCD_ERR_INVALID_ARG;
    return BCD_OK;
}

int BcdIdsEqual(const BCD_OBJECT_ID *a, const BCD_OBJECT_ID *b)
{
    if (!a || !b) return 0;
    if (a->data1 != b->data1) return 0;
    if (a->data2 != b->data2) return 0;
    if (a->data3 != b->data3) return 0;
    for (int i = 0; i < 8; ++i) {
        if (a->data4[i] != b->data4[i]) return 0;
    }
    return 1;
}

