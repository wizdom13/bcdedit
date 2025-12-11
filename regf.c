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

#define REG_TYPE_NONE 0
#define REG_TYPE_SZ 1
#define REG_TYPE_EXPAND_SZ 2
#define REG_TYPE_BINARY 3
#define REG_TYPE_DWORD 4
#define REG_TYPE_MULTI_SZ 7
#define REG_TYPE_QWORD 11

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

/* -------------------- Serialization -------------------- */

struct writer {
    unsigned char *data;
    size_t size;
    size_t capacity;
};

static int writer_reserve(struct writer *w, size_t need)
{
    if (need <= w->capacity) return 1;
    size_t newCap = w->capacity ? w->capacity * 2 : 1024;
    while (newCap < need) newCap *= 2;
    unsigned char *p = (unsigned char *)realloc(w->data, newCap);
    if (!p) return 0;
    w->data = p;
    w->capacity = newCap;
    return 1;
}

static int writer_append(struct writer *w, const void *src, size_t len)
{
    if (!writer_reserve(w, w->size + len)) return 0;
    memcpy(w->data + w->size, src, len);
    w->size += len;
    return 1;
}

static size_t align4(size_t v)
{
    return (v + 3U) & ~3U;
}

static int32_t append_cell(struct writer *w, const unsigned char *payload, size_t payloadSize)
{
    size_t cellSize = align4(payloadSize + 4);
    int32_t offset = (int32_t)w->size;
    int32_t signedSize = -(int32_t)cellSize;
    if (!writer_append(w, &signedSize, 4)) return -1;
    if (!writer_append(w, payload, payloadSize)) return -1;
    unsigned char pad[3] = {0};
    size_t padLen = cellSize - payloadSize - 4;
    if (padLen > 0 && !writer_append(w, pad, padLen)) return -1;
    return offset;
}

static int append_value_cell(struct writer *w, const char *name, uint32_t regType, const unsigned char *data, uint32_t dataSize, int32_t *outOffset)
{
    unsigned char header[0x18];
    memset(header, 0, sizeof(header));
    header[0] = 'v';
    header[1] = 'k';
    uint16_t nameLen = (uint16_t)strlen(name);
    header[0x02] = (unsigned char)(nameLen & 0xff);
    header[0x03] = (unsigned char)((nameLen >> 8) & 0xff);
    header[0x04] = (unsigned char)(dataSize & 0xff);
    header[0x05] = (unsigned char)((dataSize >> 8) & 0xff);
    header[0x06] = (unsigned char)((dataSize >> 16) & 0xff);
    header[0x07] = (unsigned char)((dataSize >> 24) & 0xff);
    int32_t dataOffset = 0;
    if (dataSize > 4) {
        dataOffset = (int32_t)align4(w->size) + (int32_t)0x1000;
    }
    header[0x08] = (unsigned char)(dataOffset & 0xff);
    header[0x09] = (unsigned char)((dataOffset >> 8) & 0xff);
    header[0x0a] = (unsigned char)((dataOffset >> 16) & 0xff);
    header[0x0b] = (unsigned char)((dataOffset >> 24) & 0xff);
    header[0x0c] = (unsigned char)(regType & 0xff);
    header[0x0d] = (unsigned char)((regType >> 8) & 0xff);
    header[0x0e] = (unsigned char)((regType >> 16) & 0xff);
    header[0x0f] = (unsigned char)((regType >> 24) & 0xff);
    unsigned char *payload = (unsigned char *)malloc(0x18 + nameLen);
    if (!payload) return 0;
    memcpy(payload, header, 0x18);
    memcpy(payload + 0x18, name, nameLen);

    int32_t offset = append_cell(w, payload, 0x18 + nameLen);
    free(payload);
    if (offset < 0) return 0;

    if (dataSize <= 4) {
        unsigned char tmp[4] = {0};
        memcpy(tmp, data, dataSize);
        memcpy(w->data + (size_t)offset + 4 + 0x08, tmp, 4);
    } else {
        size_t aligned = align4(w->size);
        if (!writer_reserve(w, aligned + dataSize)) return 0;
        memset(w->data + w->size, 0, aligned - w->size);
        w->size = aligned;
        memcpy(w->data + w->size, data, dataSize);
        w->size += dataSize;
    }

    if (outOffset) *outOffset = offset;
    return 1;
}

static int append_subkey_list(struct writer *w, const int32_t *offsets, size_t count, int32_t *outOffset)
{
    size_t payloadSize = 0x08 + count * 4;
    unsigned char *payload = (unsigned char *)calloc(1, payloadSize);
    if (!payload) return 0;
    payload[0x00] = 'l';
    payload[0x01] = 'f';
    payload[0x04] = 'l';
    payload[0x05] = 'f';
    payload[0x06] = (unsigned char)(count & 0xff);
    payload[0x07] = (unsigned char)((count >> 8) & 0xff);
    for (size_t i = 0; i < count; ++i) {
        int32_t off = offsets[i];
        payload[0x08 + i * 4] = (unsigned char)(off & 0xff);
        payload[0x09 + i * 4] = (unsigned char)((off >> 8) & 0xff);
        payload[0x0a + i * 4] = (unsigned char)((off >> 16) & 0xff);
        payload[0x0b + i * 4] = (unsigned char)((off >> 24) & 0xff);
    }
    int32_t offset = append_cell(w, payload, payloadSize);
    free(payload);
    if (offset < 0) return 0;
    if (outOffset) *outOffset = offset;
    return 1;
}

static int append_value_list(struct writer *w, const int32_t *offsets, size_t count, int32_t *outOffset)
{
    size_t payloadSize = 4 + count * 4;
    unsigned char *payload = (unsigned char *)calloc(1, payloadSize);
    if (!payload) return 0;
    for (size_t i = 0; i < count; ++i) {
        int32_t off = offsets[i];
        payload[4 + i * 4] = (unsigned char)(off & 0xff);
        payload[5 + i * 4] = (unsigned char)((off >> 8) & 0xff);
        payload[6 + i * 4] = (unsigned char)((off >> 16) & 0xff);
        payload[7 + i * 4] = (unsigned char)((off >> 24) & 0xff);
    }
    int32_t offset = append_cell(w, payload, payloadSize);
    free(payload);
    if (offset < 0) return 0;
    if (outOffset) *outOffset = offset;
    return 1;
}

static int32_t append_key(struct writer *w, const char *name, uint16_t subkeyCount, int32_t subkeyList, uint32_t valueCount, int32_t valueList)
{
    uint16_t nameLen = (uint16_t)strlen(name);
    size_t payloadSize = 0x4c + nameLen;
    unsigned char *payload = (unsigned char *)calloc(1, payloadSize);
    if (!payload) return -1;
    payload[0x00] = 'n';
    payload[0x01] = 'k';
    payload[0x04] = (unsigned char)(subkeyCount & 0xff);
    payload[0x05] = (unsigned char)((subkeyCount >> 8) & 0xff);
    payload[0x14] = (unsigned char)(subkeyCount & 0xff);
    payload[0x15] = (unsigned char)((subkeyCount >> 8) & 0xff);
    payload[0x1c] = (unsigned char)(subkeyList & 0xff);
    payload[0x1d] = (unsigned char)((subkeyList >> 8) & 0xff);
    payload[0x1e] = (unsigned char)((subkeyList >> 16) & 0xff);
    payload[0x1f] = (unsigned char)((subkeyList >> 24) & 0xff);
    payload[0x24] = (unsigned char)(valueCount & 0xff);
    payload[0x25] = (unsigned char)((valueCount >> 8) & 0xff);
    payload[0x26] = (unsigned char)((valueCount >> 16) & 0xff);
    payload[0x27] = (unsigned char)((valueCount >> 24) & 0xff);
    payload[0x28] = (unsigned char)(valueList & 0xff);
    payload[0x29] = (unsigned char)((valueList >> 8) & 0xff);
    payload[0x2a] = (unsigned char)((valueList >> 16) & 0xff);
    payload[0x2b] = (unsigned char)((valueList >> 24) & 0xff);
    payload[0x48] = (unsigned char)(nameLen & 0xff);
    payload[0x49] = (unsigned char)((nameLen >> 8) & 0xff);
    memcpy(payload + 0x4c, name, nameLen);

    int32_t offset = append_cell(w, payload, payloadSize);
    free(payload);
    return offset;
}

static uint32_t element_to_regtype(BCD_ELEMENT_KIND kind)
{
    switch (kind) {
    case BCD_ELEMENT_STRING:
        return REG_TYPE_SZ;
    case BCD_ELEMENT_INTEGER:
        return REG_TYPE_QWORD;
    case BCD_ELEMENT_BOOLEAN:
        return REG_TYPE_DWORD;
    case BCD_ELEMENT_BINARY:
    default:
        return REG_TYPE_BINARY;
    }
}

int RegfSerializeBcdStore(const BCD_STORE *store, unsigned char **outBuffer, size_t *outSize)
{
    if (!store || !outBuffer || !outSize) return BCD_ERR_INVALID_ARG;
    struct writer w = {0};

    int32_t *objectOffsets = (int32_t *)calloc(store->objectCount, sizeof(int32_t));
    if (!objectOffsets) return BCD_ERR_IO;

    for (size_t i = 0; i < store->objectCount; ++i) {
        const BCD_OBJECT *obj = &store->objects[i];
        int32_t *valueOffsets = NULL;
        if (obj->elementCount > 0) {
            valueOffsets = (int32_t *)calloc(obj->elementCount, sizeof(int32_t));
            if (!valueOffsets) { free(objectOffsets); return BCD_ERR_IO; }
        }
        for (size_t v = 0; v < obj->elementCount; ++v) {
            const BCD_ELEMENT *el = &obj->elements[v];
            char nameBuf[16];
            snprintf(nameBuf, sizeof(nameBuf), "%08x", el->type);
            uint32_t regType = element_to_regtype(el->kind);
            unsigned char dataBuf[BCD_MAX_BINARY_SIZE + 16];
            uint32_t dataSize = 0;
            switch (el->kind) {
            case BCD_ELEMENT_STRING:
                dataSize = (uint32_t)(strlen(el->data.stringValue) + 1);
                memcpy(dataBuf, el->data.stringValue, dataSize);
                break;
            case BCD_ELEMENT_BOOLEAN: {
                uint32_t val = el->data.boolValue ? 1U : 0U;
                memcpy(dataBuf, &val, sizeof(uint32_t));
                dataSize = 4;
                break;
            }
            case BCD_ELEMENT_INTEGER: {
                uint64_t qv = el->data.integerValue;
                memcpy(dataBuf, &qv, sizeof(uint64_t));
                dataSize = 8;
                regType = REG_TYPE_QWORD;
                break;
            }
            case BCD_ELEMENT_BINARY:
            default:
                dataSize = (uint32_t)el->data.binaryValue.size;
                memcpy(dataBuf, el->data.binaryValue.data, dataSize);
                regType = REG_TYPE_BINARY;
                break;
            }
            if (!append_value_cell(&w, nameBuf, regType, dataBuf, dataSize, &valueOffsets[v])) {
                free(valueOffsets);
                free(objectOffsets);
                free(w.data);
                return BCD_ERR_IO;
            }
        }

        int32_t valueListOff = 0;
        if (obj->elementCount > 0) {
            if (!append_value_list(&w, valueOffsets, obj->elementCount, &valueListOff)) {
                free(valueOffsets);
                free(objectOffsets);
                free(w.data);
                return BCD_ERR_IO;
            }
        }
        free(valueOffsets);

        char nameBuf[64];
        if (BcdFormatObjectId(&obj->id, nameBuf, sizeof(nameBuf)) != BCD_OK) {
            free(objectOffsets);
            free(w.data);
            return BCD_ERR_IO;
        }
        objectOffsets[i] = append_key(&w, nameBuf, 0, 0, (uint32_t)obj->elementCount, valueListOff);
        if (objectOffsets[i] < 0) {
            free(objectOffsets);
            free(w.data);
            return BCD_ERR_IO;
        }
    }

    int32_t subkeyList = 0;
    if (store->objectCount > 0) {
        if (!append_subkey_list(&w, objectOffsets, store->objectCount, &subkeyList)) {
            free(objectOffsets);
            free(w.data);
            return BCD_ERR_IO;
        }
    }
    free(objectOffsets);

    int32_t rootKey = append_key(&w, "Objects", (uint16_t)store->objectCount, subkeyList, 0, 0);
    if (rootKey < 0) {
        free(w.data);
        return BCD_ERR_IO;
    }

    size_t hiveSize = 0x1000 + w.size;
    size_t padded = align4(hiveSize);
    unsigned char *buffer = (unsigned char *)calloc(1, padded);
    if (!buffer) {
        free(w.data);
        return BCD_ERR_IO;
    }

    memcpy(buffer, "regf", 4);
    buffer[0x24] = (unsigned char)(rootKey & 0xff);
    buffer[0x25] = (unsigned char)((rootKey >> 8) & 0xff);
    buffer[0x26] = (unsigned char)((rootKey >> 16) & 0xff);
    buffer[0x27] = (unsigned char)((rootKey >> 24) & 0xff);

    memcpy(buffer + 0x1000, w.data, w.size);
    *outBuffer = buffer;
    *outSize = padded;
    free(w.data);
    return BCD_OK;
}
