#ifndef REGF_H
#define REGF_H

#include <stddef.h>
#include <stdint.h>

typedef struct REGF_HIVE REGF_HIVE;

typedef struct REGF_KEY {
    const unsigned char *cell;
    size_t cellSize;
    const char *name;
    uint16_t nameLen;
    int subkeyCount;
    int valueCount;
    int *subkeyOffsets;
    int *valueOffsets;
    REGF_HIVE *hive;
} REGF_KEY;

typedef struct REGF_VALUE {
    const unsigned char *cell;
    size_t cellSize;
    const char *name;
    uint16_t nameLen;
    uint32_t dataSize;
    uint32_t dataOffset;
    uint32_t type;
    REGF_HIVE *hive;
} REGF_VALUE;

REGF_HIVE *RegfOpen(const unsigned char *buffer, size_t size);
void RegfClose(REGF_HIVE *hive);

REGF_KEY *RegfGetRootKey(REGF_HIVE *hive);
REGF_KEY *RegfFindSubKey(REGF_KEY *parent, const char *name);
int RegfGetSubKeyCount(REGF_KEY *key);
REGF_KEY *RegfGetSubKeyAt(REGF_KEY *key, int index);
int RegfGetValueCount(REGF_KEY *key);
REGF_VALUE *RegfGetValueAt(REGF_KEY *key, int index);

const char *RegfGetKeyName(REGF_KEY *key);
const char *RegfGetValueName(REGF_VALUE *value);
uint32_t RegfGetValueType(REGF_VALUE *value);
const void *RegfGetValueData(REGF_VALUE *value, size_t *size);
uint32_t RegfGetValueDataAsUint32(REGF_VALUE *value, int *ok);

void RegfReleaseKey(REGF_KEY *key);
void RegfReleaseValue(REGF_VALUE *value);

#endif /* REGF_H */
