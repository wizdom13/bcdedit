#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bcd.h"
#include "regf.h"
#include "bcd_parser.h"

typedef enum {
    CMD_NONE,
    CMD_ENUM,
    CMD_EXPORT,
    CMD_HELP
} COMMAND_TYPE;

typedef struct _OPTIONS {
    const char *storePath;
    COMMAND_TYPE command;
    const char *exportPath;
    char objectIdText[80];
} OPTIONS;

static void print_usage(void)
{
    printf("Minimal read-only bcdedit clone\n");
    printf("Usage:\n");
    printf("  bcdedit /?\n");
    printf("  bcdedit /store <path> /enum [<id>]\n");
    printf("  bcdedit /store <path> /export <output> [<id>]\n");
    printf("\nSupported features (read):\n");
    printf("  /enum [<id>]           enumerate the entire store or a single object\n");
    printf("\nSupported features (write):\n");
    printf("  /export <output>       write a text representation of the store (optionally filtered by <id>)\n");
}

static int parse_options(int argc, char **argv, OPTIONS *opts)
{
    memset(opts, 0, sizeof(*opts));
    opts->command = CMD_NONE;
    if (argc < 2) return -1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "/?") == 0 || strcmp(argv[i], "/help") == 0) {
            opts->command = CMD_HELP;
        } else if (strcmp(argv[i], "/store") == 0) {
            if (i + 1 >= argc) return -1;
            opts->storePath = argv[++i];
        } else if (strcmp(argv[i], "/enum") == 0) {
            opts->command = CMD_ENUM;
            if (i + 1 < argc && argv[i + 1][0] == '{') {
                strncpy(opts->objectIdText, argv[i + 1], sizeof(opts->objectIdText) - 1);
                opts->objectIdText[sizeof(opts->objectIdText) - 1] = '\0';
                ++i;
            }
        } else if (strcmp(argv[i], "/export") == 0) {
            opts->command = CMD_EXPORT;
            if (i + 1 >= argc) return -1;
            opts->exportPath = argv[++i];
            if (i + 1 < argc && argv[i + 1][0] == '{') {
                strncpy(opts->objectIdText, argv[i + 1], sizeof(opts->objectIdText) - 1);
                opts->objectIdText[sizeof(opts->objectIdText) - 1] = '\0';
                ++i;
            }
        }
    }
    if (opts->command == CMD_ENUM && !opts->storePath) return -1;
    if (opts->command == CMD_EXPORT && (!opts->storePath || !opts->exportPath)) return -1;
    if (opts->command == CMD_NONE) return -1;
    return 0;
}

static int load_bcd_store(const char *path, BCD_STORE *store, REGF_HIVE **outHive, unsigned char **outBuffer, size_t *outSize)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open %s\n", path);
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long size = ftell(f);
    if (size <= 0) {
        fclose(f);
        return -1;
    }
    rewind(f);
    unsigned char *buffer = (unsigned char *)malloc((size_t)size);
    if (!buffer) {
        fclose(f);
        return -1;
    }
    if (fread(buffer, 1, (size_t)size, f) != (size_t)size) {
        free(buffer);
        fclose(f);
        return -1;
    }
    fclose(f);

    REGF_HIVE *hive = RegfOpen(buffer, (size_t)size);
    if (!hive) {
        fprintf(stderr, "Invalid hive format\n");
        free(buffer);
        return -1;
    }

    if (BcdStoreInit(store) != BCD_OK) {
        RegfClose(hive);
        free(buffer);
        return -1;
    }
    if (BcdStoreLoadFromHive(store, hive) != BCD_OK) {
        fprintf(stderr, "Failed to parse BCD store\n");
        RegfClose(hive);
        free(buffer);
        return -1;
    }

    *outHive = hive;
    *outBuffer = buffer;
    *outSize = (size_t)size;
    return 0;
}

static void print_element(FILE *out, const BCD_ELEMENT *el)
{
    if (!el) return;
    fprintf(out, "    element 0x%08x : ", el->type);
    switch (el->kind) {
    case BCD_ELEMENT_INTEGER:
        fprintf(out, "integer %llu\n", (unsigned long long)el->data.integerValue);
        break;
    case BCD_ELEMENT_STRING:
        fprintf(out, "string \"%s\"\n", el->data.stringValue);
        break;
    case BCD_ELEMENT_BOOLEAN:
        fprintf(out, "boolean %s\n", el->data.boolValue ? "true" : "false");
        break;
    case BCD_ELEMENT_BINARY:
        fprintf(out, "binary (%zu bytes)\n", el->data.binaryValue.size);
        break;
    default:
        fprintf(out, "unknown\n");
        break;
    }
}

static void print_object(FILE *out, const BCD_OBJECT *obj)
{
    char idText[64];
    if (BcdFormatObjectId(&obj->id, idText, sizeof(idText)) != BCD_OK) {
        strcpy(idText, "{invalid}");
    }
    fprintf(out, "----------------------------------------\n");
    fprintf(out, "identifier              %s\n", idText);
    fprintf(out, "type                    0x%08x\n", obj->objectType);
    for (size_t i = 0; i < obj->elementCount; ++i) {
        print_element(out, &obj->elements[i]);
    }
}

static int cmd_enum(const OPTIONS *opts, BCD_STORE *store)
{
    if (!opts || !store) return -1;
    if (opts->objectIdText[0]) {
        BCD_OBJECT_ID id;
        if (BcdParseObjectId(opts->objectIdText, &id) != BCD_OK) {
            fprintf(stderr, "Invalid object identifier format\n");
            return -1;
        }
        BCD_OBJECT *obj = BcdStoreFindObjectById(store, &id);
        if (!obj) {
            fprintf(stderr, "Object not found\n");
            return -1;
        }
        print_object(stdout, obj);
        return 0;
    }

    size_t count = BcdStoreGetObjectCount(store);
    for (size_t i = 0; i < count; ++i) {
        BCD_OBJECT *obj = BcdStoreGetObjectAt(store, i);
        if (obj) print_object(stdout, obj);
    }
    return 0;
}

static int cmd_export(const OPTIONS *opts, BCD_STORE *store)
{
    if (!opts || !store || !opts->exportPath) return -1;

    FILE *out = fopen(opts->exportPath, "w");
    if (!out) {
        fprintf(stderr, "Failed to open %s for writing\n", opts->exportPath);
        return -1;
    }

    int result = 0;
    if (opts->objectIdText[0]) {
        BCD_OBJECT_ID id;
        if (BcdParseObjectId(opts->objectIdText, &id) != BCD_OK) {
            fprintf(stderr, "Invalid object identifier format\n");
            result = -1;
        } else {
            BCD_OBJECT *obj = BcdStoreFindObjectById(store, &id);
            if (!obj) {
                fprintf(stderr, "Object not found\n");
                result = -1;
            } else {
                print_object(out, obj);
            }
        }
    } else {
        size_t count = BcdStoreGetObjectCount(store);
        for (size_t i = 0; i < count; ++i) {
            BCD_OBJECT *obj = BcdStoreGetObjectAt(store, i);
            if (obj) print_object(out, obj);
        }
    }

    fclose(out);
    return result;
}

int main(int argc, char **argv)
{
    OPTIONS opts;
    if (parse_options(argc, argv, &opts) != 0) {
        print_usage();
        return 1;
    }

    if (opts.command == CMD_HELP) {
        print_usage();
        return 0;
    }

    REGF_HIVE *hive = NULL;
    unsigned char *buffer = NULL;
    size_t size = 0;
    BCD_STORE store;
    if (load_bcd_store(opts.storePath, &store, &hive, &buffer, &size) != 0) {
        return 1;
    }

    int result = 0;
    if (opts.command == CMD_ENUM) {
        result = cmd_enum(&opts, &store);
    } else if (opts.command == CMD_EXPORT) {
        result = cmd_export(&opts, &store);
    }

    RegfClose(hive);
    free(buffer);
    return result;
}

