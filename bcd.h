#ifndef BCD_H
#define BCD_H

#include <stddef.h>
#include <stdint.h>

#define BCD_MAX_OBJECTS 128
#define BCD_MAX_ELEMENTS_PER_OBJECT 64
#define BCD_MAX_STRING_LEN 256
#define BCD_MAX_BINARY_SIZE 1024

#define BCD_OK 0
#define BCD_ERR_INVALID_ARG -1
#define BCD_ERR_NOT_FOUND -2
#define BCD_ERR_CAPACITY -3
#define BCD_ERR_PARSE -4
#define BCD_ERR_IO -5

/* Common object types (not exhaustive). */
#define BCD_OBJECT_BOOTMGR 0x10100002U
#define BCD_OBJECT_OSLOADER 0x10200003U
#define BCD_OBJECT_RESUME 0x10300006U
#define BCD_OBJECT_INHERITANCE 0x12000004U

/* Frequently used element identifiers (subset). */
#define BCD_ELEMENT_DESCRIPTION 0x12000004U
#define BCD_ELEMENT_APPLICATION_DEVICE 0x11000001U
#define BCD_ELEMENT_APPLICATION_PATH 0x12000002U
#define BCD_ELEMENT_OSDEVICE 0x21000001U
#define BCD_ELEMENT_SYSTEMROOT 0x22000002U
#define BCD_ELEMENT_LOCALE 0x12000005U
#define BCD_ELEMENT_INHERIT 0x14000003U
#define BCD_ELEMENT_RECOVERY_SEQUENCE 0x24000001U
#define BCD_ELEMENT_DISPLAY_ORDER 0x24000002U
#define BCD_ELEMENT_BOOT_SEQUENCE 0x24000003U
#define BCD_ELEMENT_TOOLS_DISPLAY_ORDER 0x24000004U
#define BCD_ELEMENT_TIMEOUT 0x25000004U
#define BCD_ELEMENT_BOOTMANAGER_DEFAULT 0x23000003U
#define BCD_ELEMENT_BOOLEAN_BOOTDEBUG 0x26000010U
#define BCD_ELEMENT_BOOLEAN_BOOTEMS 0x26000020U
#define BCD_ELEMENT_BOOLEAN_EMS 0x26000022U
#define BCD_ELEMENT_BOOLEAN_DEBUG 0x260000E0U

#define BCD_ID_STRING_LENGTH 38

#ifdef __cplusplus
extern "C" {
#endif

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

/* Mapping helpers */
typedef struct BCD_ELEMENT_META {
    const char *name;
    uint32_t id;
    BCD_ELEMENT_KIND kind;
} BCD_ELEMENT_META;

int BcdStoreInit(BCD_STORE *store);
void BcdStoreReset(BCD_STORE *store);
size_t BcdStoreGetObjectCount(const BCD_STORE *store);
BCD_OBJECT *BcdStoreGetObjectAt(BCD_STORE *store, size_t index);
BCD_OBJECT *BcdStoreFindObjectById(BCD_STORE *store, const BCD_OBJECT_ID *id);
int BcdStoreAddObject(BCD_STORE *store, const BCD_OBJECT *object);
int BcdStoreDeleteObject(BCD_STORE *store, const BCD_OBJECT_ID *id);

int BcdGenerateObjectId(BCD_OBJECT_ID *id);
int BcdParseObjectId(const char *text, BCD_OBJECT_ID *outId);
int BcdFormatObjectId(const BCD_OBJECT_ID *id, char *buffer, size_t bufferSize);
int BcdIdsEqual(const BCD_OBJECT_ID *a, const BCD_OBJECT_ID *b);

int BcdObjectAddElement(BCD_OBJECT *object, const BCD_ELEMENT *element);
BCD_ELEMENT *BcdObjectFindElement(BCD_OBJECT *object, uint32_t elementType);
int BcdObjectSetElement(BCD_OBJECT *object, const BCD_ELEMENT *element);
int BcdObjectRemoveElement(BCD_OBJECT *object, uint32_t elementType);

const BCD_ELEMENT_META *BcdLookupElementByName(const char *name);
const BCD_ELEMENT_META *BcdLookupElementById(uint32_t id);

#ifdef __cplusplus
}
#endif

#endif /* BCD_H */
