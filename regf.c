#include "regf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct REGF_HIVE {
    const unsigned char *buffer;
    size_t size;
    REGF_KEY *root;
};

#define HVIEW(hive, off) ((off) < 0 ? NULL : (((size_t)(off) + 0x1000 <= (hive)->size) ? (hive)->buffer + (off) + 0x1000 : NULL))

static int32_t read_int32(const unsigned char *p)
{
    return (int32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

static uint32_t read_uint32(const unsigned char *p)
{
    return (uint32_t)read_int32(p);
}

static uint16_t read_uint16(const unsigned char *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static const unsigned char *get_cell(REGF_HIVE *hive, int32_t offset, size_t *cellSize)
{
    const unsigned char *ptr = HVIEW(hive, offset);
    if (!ptr) return NULL;
    int32_t sizeSigned = read_int32(ptr);
    size_t size = (sizeSigned < 0) ? (size_t)(-sizeSigned) : (size_t)sizeSigned;
    if (size < 4 || (offset + 0x1000 + size) > hive->size) return NULL;
    if (cellSize) *cellSize = size;
    return ptr;
}

static REGF_KEY *alloc_key(REGF_HIVE *hive)
{
    REGF_KEY *key = (REGF_KEY *)calloc(1, sizeof(REGF_KEY));
    if (!key) return NULL;
    key->hive = hive;
    return key;
}

static REGF_VALUE *alloc_value(REGF_HIVE *hive)
{
    REGF_VALUE *val = (REGF_VALUE *)calloc(1, sizeof(REGF_VALUE));
    if (!val) return NULL;
    val->hive = hive;
    return val;
}

static REGF_KEY *parse_key(REGF_HIVE *hive, const unsigned char *cell, size_t cellSize)
{
    if (!cell || cellSize < 0x50) return NULL;
    if (cell[4] != 'n' || cell[5] != 'k') return NULL;
    REGF_KEY *key = alloc_key(hive);
    if (!key) return NULL;
    key->cell = cell;
    key->cellSize = cellSize;
    key->subkeyCount = read_uint16(cell + 0x14);
    key->valueCount = read_uint32(cell + 0x24);
    key->nameLen = read_uint16(cell + 0x48);
    {
        size_t needed = 0x4c + (size_t)key->nameLen;
        if (needed > cellSize) {
            RegfReleaseKey(key);
            return NULL;
        }
    }
    key->name = (const char *)(cell + 0x4c);

    if (key->subkeyCount > 0) {
        size_t listSize = 0;
        const unsigned char *listCell = get_cell(hive, read_int32(cell + 0x1c), &listSize);
        if (listCell && listSize >= 0x08 && listCell[4] == 'l' && listCell[5] == 'f') {
            int count = read_uint16(listCell + 0x06);
            if (count > 0 && (size_t)(0x08 + count * 4) <= listSize) {
                key->subkeyOffsets = (int *)calloc((size_t)count, sizeof(int));
                if (key->subkeyOffsets) {
                    for (int i = 0; i < count; ++i) {
                        key->subkeyOffsets[i] = read_int32(listCell + 0x08 + i * 4);
                    }
                    key->subkeyCount = count;
                }
            }
        }
    }

    if (key->valueCount > 0) {
        size_t listSize = 0;
        const unsigned char *listCell = get_cell(hive, read_int32(cell + 0x28), &listSize);
        if (listCell && listSize >= 4 && listSize >= 4 + (size_t)key->valueCount * 4) {
            key->valueOffsets = (int *)calloc((size_t)key->valueCount, sizeof(int));
            if (key->valueOffsets) {
                for (int i = 0; i < key->valueCount; ++i) {
                    key->valueOffsets[i] = read_int32(listCell + 4 + i * 4);
                }
            }
        }
    }

    return key;
}

static REGF_VALUE *parse_value(REGF_HIVE *hive, const unsigned char *cell, size_t cellSize)
{
    if (!cell || cellSize < 0x18) return NULL;
    if (cell[4] != 'v' || cell[5] != 'k') return NULL;
    REGF_VALUE *val = alloc_value(hive);
    if (!val) return NULL;
    val->cell = cell;
    val->cellSize = cellSize;
    val->nameLen = read_uint16(cell + 0x02);
    val->type = read_uint32(cell + 0x0c);
    val->dataSize = read_uint32(cell + 0x04);
    val->dataOffset = read_uint32(cell + 0x08);
    {
        size_t needed = 0x18 + (size_t)val->nameLen;
        if (needed > cellSize) {
            RegfReleaseValue(val);
            return NULL;
        }
    }
    val->name = (const char *)(cell + 0x18);
    return val;
}

REGF_HIVE *RegfOpen(const unsigned char *buffer, size_t size)
{
    if (!buffer || size < 4096) return NULL;
    if (memcmp(buffer, "regf", 4) != 0) return NULL;
    REGF_HIVE *hive = (REGF_HIVE *)calloc(1, sizeof(REGF_HIVE));
    if (!hive) return NULL;
    hive->buffer = buffer;
    hive->size = size;

    size_t rootCellSize = 0;
    int32_t rootOffset = read_int32(buffer + 0x24);
    const unsigned char *rootCell = get_cell(hive, rootOffset, &rootCellSize);
    hive->root = parse_key(hive, rootCell, rootCellSize);
    if (!hive->root) {
        RegfClose(hive);
        return NULL;
    }
    return hive;
}

void RegfClose(REGF_HIVE *hive)
{
    if (!hive) return;
    if (hive->root) RegfReleaseKey(hive->root);
    free(hive);
}

REGF_KEY *RegfGetRootKey(REGF_HIVE *hive)
{
    return hive ? hive->root : NULL;
}

REGF_KEY *RegfFindSubKey(REGF_KEY *parent, const char *name)
{
    if (!parent || !name) return NULL;
    int count = RegfGetSubKeyCount(parent);
    for (int i = 0; i < count; ++i) {
        REGF_KEY *child = RegfGetSubKeyAt(parent, i);
        if (child && child->nameLen == strlen(name) && strncmp(child->name, name, child->nameLen) == 0) {
            return child;
        }
        RegfReleaseKey(child);
    }
    return NULL;
}

int RegfGetSubKeyCount(REGF_KEY *key)
{
    return key ? key->subkeyCount : 0;
}

REGF_KEY *RegfGetSubKeyAt(REGF_KEY *key, int index)
{
    if (!key || !key->subkeyOffsets || index < 0 || index >= key->subkeyCount) return NULL;
    size_t cellSize = 0;
    const unsigned char *cell = get_cell(key->hive, key->subkeyOffsets[index], &cellSize);
    return parse_key(key->hive, cell, cellSize);
}

int RegfGetValueCount(REGF_KEY *key)
{
    return key ? key->valueCount : 0;
}

REGF_VALUE *RegfGetValueAt(REGF_KEY *key, int index)
{
    if (!key || !key->valueOffsets || index < 0 || index >= key->valueCount) return NULL;
    size_t cellSize = 0;
    const unsigned char *cell = get_cell(key->hive, key->valueOffsets[index], &cellSize);
    return parse_value(key->hive, cell, cellSize);
}

const char *RegfGetKeyName(REGF_KEY *key)
{
    if (!key) return NULL;
    static char nameBuf[256];
    size_t len = key->nameLen < sizeof(nameBuf) - 1 ? key->nameLen : sizeof(nameBuf) - 1;
    memcpy(nameBuf, key->name, len);
    nameBuf[len] = '\0';
    return nameBuf;
}

const char *RegfGetValueName(REGF_VALUE *value)
{
    if (!value) return NULL;
    static char nameBuf[256];
    size_t len = value->nameLen < sizeof(nameBuf) - 1 ? value->nameLen : sizeof(nameBuf) - 1;
    memcpy(nameBuf, value->name, len);
    nameBuf[len] = '\0';
    return nameBuf;
}

uint32_t RegfGetValueType(REGF_VALUE *value)
{
    return value ? value->type : 0;
}

const void *RegfGetValueData(REGF_VALUE *value, size_t *size)
{
    if (!value || value->dataSize == 0) return NULL;
    if (value->dataSize <= 4) {
        if (size) *size = value->dataSize;
        return value->cell + 0x08;
    }
    size_t offset = (size_t)value->dataOffset + 0x1000;
    if (offset + value->dataSize > value->hive->size) return NULL;
    if (size) *size = value->dataSize;
    return value->hive->buffer + offset;
}

uint32_t RegfGetValueDataAsUint32(REGF_VALUE *value, int *ok)
{
    if (ok) *ok = 0;
    size_t size = 0;
    const void *data = RegfGetValueData(value, &size);
    if (!data || size < 4) return 0;
    if (ok) *ok = 1;
    const unsigned char *p = (const unsigned char *)data;
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

void RegfReleaseKey(REGF_KEY *key)
{
    if (!key) return;
    if (key->subkeyOffsets) free(key->subkeyOffsets);
    if (key->valueOffsets) free(key->valueOffsets);
    free(key);
}

void RegfReleaseValue(REGF_VALUE *value)
{
    if (!value) return;
    free(value);
}

