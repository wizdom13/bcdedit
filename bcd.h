#ifndef BCD_H
#define BCD_H

#include <stddef.h>
#include <stdint.h>

#define BCD_MAX_OBJECTS 64
#define BCD_MAX_ELEMENTS_PER_OBJECT 32
#define BCD_MAX_STRING_LEN 256
#define BCD_MAX_BINARY_SIZE 512

#define BCD_OK 0
#define BCD_ERR_INVALID_ARG -1
#define BCD_ERR_NOT_FOUND -2
#define BCD_ERR_CAPACITY -3
#define BCD_ERR_PARSE -4

typedef struct BCD_OBJECT_ID {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
} BCD_OBJECT_ID;

typedef enum {
    BCD_ELEMENT_UNKNOWN = 0,
    BCD_ELEMENT_INTEGER,
    BCD_ELEMENT_STRING,
    BCD_ELEMENT_BOOLEAN,
    BCD_ELEMENT_BINARY
} BCD_ELEMENT_KIND;

typedef struct BCD_ELEMENT {
    uint32_t type;
    BCD_ELEMENT_KIND kind;
    union {
        uint64_t integerValue;
        char stringValue[BCD_MAX_STRING_LEN];
        int boolValue;
        struct {
            uint8_t data[BCD_MAX_BINARY_SIZE];
            size_t size;
        } binaryValue;
    } data;
} BCD_ELEMENT;

typedef struct BCD_OBJECT {
    BCD_OBJECT_ID id;
    uint32_t objectType;
    BCD_ELEMENT elements[BCD_MAX_ELEMENTS_PER_OBJECT];
    size_t elementCount;
} BCD_OBJECT;

typedef struct BCD_STORE {
    BCD_OBJECT objects[BCD_MAX_OBJECTS];
    size_t objectCount;
} BCD_STORE;

int BcdStoreInit(BCD_STORE *store);
void BcdStoreReset(BCD_STORE *store);
int BcdStoreAddObject(BCD_STORE *store, const BCD_OBJECT *object);
BCD_OBJECT *BcdStoreFindObjectById(BCD_STORE *store, const BCD_OBJECT_ID *id);
size_t BcdStoreGetObjectCount(const BCD_STORE *store);
BCD_OBJECT *BcdStoreGetObjectAt(BCD_STORE *store, size_t index);

int BcdObjectAddElement(BCD_OBJECT *object, const BCD_ELEMENT *element);
BCD_ELEMENT *BcdObjectFindElement(BCD_OBJECT *object, uint32_t elementType);

int BcdParseObjectId(const char *text, BCD_OBJECT_ID *outId);
int BcdFormatObjectId(const BCD_OBJECT_ID *id, char *buffer, size_t bufferSize);
int BcdIdsEqual(const BCD_OBJECT_ID *a, const BCD_OBJECT_ID *b);

#endif /* BCD_H */
