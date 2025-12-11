#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bcd.h"
#include "regf.h"
#include "bcd_parser.h"

#ifndef _WIN32
#define DEFAULT_SYSTEM_STORE NULL
#else
#include <windows.h>
#endif

typedef enum {
    CMD_HELP,
    CMD_ENUM,
    CMD_EXPORT,
    CMD_IMPORT,
    CMD_CREATESTORE,
    CMD_CREATE,
    CMD_COPY,
    CMD_DELETE,
    CMD_SET,
    CMD_DELETEVALUE,
    CMD_DEFAULT,
    CMD_TIMEOUT,
    CMD_DISPLAYORDER,
    CMD_BOOTSEQUENCE,
    CMD_TOOLSDISPLAYORDER,
    CMD_UNKNOWN
} COMMAND_TYPE;

typedef struct OPTIONS {
    const char *storePath;
    COMMAND_TYPE command;
    const char *pathArg;
    char idText[64];
    char targetIdText[64];
    const char *elementName;
    const char **extraValues;
    int extraCount;
    int verbose;
    const char *application;
    const char *description;
} OPTIONS;

static void print_usage_summary(void)
{
    printf("bcdedit-style tool (clean-room)\n");
    printf("Common commands:\n");
    printf("  bcdedit /? [command]             Show help\n");
    printf("  bcdedit /enum [type] [/v]        Enumerate entries\n");
    printf("  bcdedit /createstore <file>      Create empty store\n");
    printf("  bcdedit /import <file>           Replace system/offline store with file contents\n");
    printf("  bcdedit /export <file>           Export store to hive file\n");
    printf("  bcdedit /create {id|/d desc /application type}   Create new entry\n");
    printf("  bcdedit /copy <id> /d desc       Duplicate entry\n");
    printf("  bcdedit /delete <id>             Remove entry\n");
    printf("  bcdedit /set <id> <element> <value...>  Set element\n");
    printf("  bcdedit /deletevalue <id> <element>     Remove element\n");
    printf("  bcdedit /default <id>            Set default entry\n");
    printf("  bcdedit /timeout <seconds>       Set boot timeout\n");
}

static void print_usage_command(const char *cmd)
{
    if (!cmd) return;
    if (strcmp(cmd, "enum") == 0) {
        printf("/enum [all|active|bootmgr|osloader] [/v]\n");
    } else if (strcmp(cmd, "create") == 0) {
        printf("/create {<id>|/d <description> /application <type>}\n");
    } else if (strcmp(cmd, "set") == 0) {
        printf("/set <id> <element> <value> ...\n");
    } else {
        print_usage_summary();
    }
}

static int parse_options(int argc, char **argv, OPTIONS *opts)
{
    memset(opts, 0, sizeof(*opts));
    opts->command = CMD_UNKNOWN;
    opts->extraValues = NULL;
    opts->extraCount = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "/?") == 0 || strcmp(argv[i], "/help") == 0) {
            opts->command = CMD_HELP;
            if (i + 1 < argc) print_usage_command(argv[i + 1]);
            return 0;
        } else if (strcmp(argv[i], "/store") == 0) {
            if (i + 1 >= argc) return -1;
            opts->storePath = argv[++i];
        } else if (strcmp(argv[i], "/enum") == 0) {
            opts->command = CMD_ENUM;
        } else if (strcmp(argv[i], "/export") == 0) {
            opts->command = CMD_EXPORT;
            if (i + 1 >= argc) return -1;
            opts->pathArg = argv[++i];
        } else if (strcmp(argv[i], "/import") == 0) {
            opts->command = CMD_IMPORT;
            if (i + 1 >= argc) return -1;
            opts->pathArg = argv[++i];
        } else if (strcmp(argv[i], "/createstore") == 0) {
            opts->command = CMD_CREATESTORE;
            if (i + 1 >= argc) return -1;
            opts->pathArg = argv[++i];
        } else if (strcmp(argv[i], "/create") == 0) {
            opts->command = CMD_CREATE;
            if (i + 1 < argc && argv[i + 1][0] == '{') {
                strncpy(opts->idText, argv[i + 1], sizeof(opts->idText) - 1);
                ++i;
            }
        } else if (strcmp(argv[i], "/copy") == 0) {
            opts->command = CMD_COPY;
            if (i + 1 >= argc) return -1;
            strncpy(opts->idText, argv[++i], sizeof(opts->idText) - 1);
        } else if (strcmp(argv[i], "/delete") == 0) {
            opts->command = CMD_DELETE;
            if (i + 1 >= argc) return -1;
            strncpy(opts->idText, argv[++i], sizeof(opts->idText) - 1);
        } else if (strcmp(argv[i], "/set") == 0) {
            opts->command = CMD_SET;
            if (i + 2 >= argc) return -1;
            strncpy(opts->idText, argv[++i], sizeof(opts->idText) - 1);
            opts->elementName = argv[++i];
            opts->extraValues = (const char **)&argv[i + 1];
            opts->extraCount = argc - i - 1;
            break;
        } else if (strcmp(argv[i], "/deletevalue") == 0) {
            opts->command = CMD_DELETEVALUE;
            if (i + 2 >= argc) return -1;
            strncpy(opts->idText, argv[++i], sizeof(opts->idText) - 1);
            opts->elementName = argv[++i];
        } else if (strcmp(argv[i], "/default") == 0) {
            opts->command = CMD_DEFAULT;
            if (i + 1 >= argc) return -1;
            strncpy(opts->targetIdText, argv[++i], sizeof(opts->targetIdText) - 1);
        } else if (strcmp(argv[i], "/timeout") == 0) {
            opts->command = CMD_TIMEOUT;
            if (i + 1 >= argc) return -1;
            opts->elementName = argv[++i];
        } else if (strcmp(argv[i], "/displayorder") == 0) {
            opts->command = CMD_DISPLAYORDER;
            if (i + 1 >= argc) return -1;
            opts->extraValues = (const char **)&argv[i + 1];
            opts->extraCount = argc - i - 1;
            break;
        } else if (strcmp(argv[i], "/bootsequence") == 0) {
            opts->command = CMD_BOOTSEQUENCE;
            if (i + 1 >= argc) return -1;
            opts->extraValues = (const char **)&argv[i + 1];
            opts->extraCount = argc - i - 1;
            break;
        } else if (strcmp(argv[i], "/toolsdisplayorder") == 0) {
            opts->command = CMD_TOOLSDISPLAYORDER;
            if (i + 1 >= argc) return -1;
            opts->extraValues = (const char **)&argv[i + 1];
            opts->extraCount = argc - i - 1;
            break;
        } else if (strcmp(argv[i], "/d") == 0) {
            if (i + 1 >= argc) return -1;
            opts->description = argv[++i];
        } else if (strcmp(argv[i], "/application") == 0) {
            if (i + 1 >= argc) return -1;
            opts->application = argv[++i];
        } else if (strcmp(argv[i], "/v") == 0) {
            opts->verbose = 1;
        }
    }

    if (opts->command == CMD_UNKNOWN) opts->command = CMD_ENUM;
    return 0;
}

static const char *resolve_system_store(void)
{
#ifdef _WIN32
    static char path[MAX_PATH];
    const char *root = getenv("SystemRoot");
    if (!root) root = "C:\\Windows";
    snprintf(path, sizeof(path), "%s\\Boot\\BCD", root);
    return path;
#else
    return NULL;
#endif
}

static int read_file(const char *path, unsigned char **buffer, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return BCD_ERR_IO;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return BCD_ERR_IO; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return BCD_ERR_IO; }
    rewind(f);
    unsigned char *buf = (unsigned char *)malloc((size_t)sz);
    if (!buf) { fclose(f); return BCD_ERR_IO; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return BCD_ERR_IO; }
    fclose(f);
    *buffer = buf;
    *size = (size_t)sz;
    return BCD_OK;
}

static int write_file(const char *path, const unsigned char *buffer, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (!f) return BCD_ERR_IO;
    if (fwrite(buffer, 1, size, f) != size) { fclose(f); return BCD_ERR_IO; }
    fclose(f);
    return BCD_OK;
}

static int load_bcd_store(const char *path, BCD_STORE *store)
{
    unsigned char *buffer = NULL;
    size_t size = 0;
    if (read_file(path, &buffer, &size) != BCD_OK) {
        fprintf(stderr, "Failed to open store: %s\n", path);
        return BCD_ERR_IO;
    }
    REGF_HIVE *hive = RegfOpen(buffer, size);
    if (!hive) {
        fprintf(stderr, "Invalid hive file: %s\n", path);
        free(buffer);
        return BCD_ERR_PARSE;
    }
    if (BcdStoreInit(store) != BCD_OK) {
        RegfClose(hive);
        free(buffer);
        return BCD_ERR_INVALID_ARG;
    }
    int status = BcdStoreLoadFromHive(store, hive);
    RegfClose(hive);
    free(buffer);
    return status;
}

static int save_bcd_store(const char *path, const BCD_STORE *store)
{
    unsigned char *buffer = NULL;
    size_t size = 0;
    int status = BcdStoreSerializeToHive(store, &buffer, &size);
    if (status != BCD_OK) return status;
    status = write_file(path, buffer, size);
    free(buffer);
    return status;
}

static void print_element(const BCD_ELEMENT *el, int verbose)
{
    if (!el) return;
    const BCD_ELEMENT_META *meta = BcdLookupElementById(el->type);
    if (verbose && meta) printf("  %s (0x%08x): ", meta->name, el->type);
    else if (meta) printf("  %s: ", meta->name);
    else printf("  0x%08x: ", el->type);

    switch (el->kind) {
    case BCD_ELEMENT_INTEGER:
        printf("%llu\n", (unsigned long long)el->data.integerValue);
        break;
    case BCD_ELEMENT_STRING:
        printf("%s\n", el->data.stringValue);
        break;
    case BCD_ELEMENT_BOOLEAN:
        printf(el->data.boolValue ? "ON\n" : "OFF\n");
        break;
    case BCD_ELEMENT_BINARY:
        printf("%zu bytes\n", el->data.binaryValue.size);
        break;
    default:
        printf("unknown\n");
        break;
    }
}

static void print_object(const BCD_OBJECT *obj, int verbose)
{
    char idText[64];
    if (BcdFormatObjectId(&obj->id, idText, sizeof(idText)) != BCD_OK) strcpy(idText, "{invalid}");
    printf("identifier %s\n", idText);
    if (verbose) printf("type 0x%08x\n", obj->objectType);
    for (size_t i = 0; i < obj->elementCount; ++i) {
        print_element(&obj->elements[i], verbose);
    }
    printf("\n");
}

static int cmd_enum(const OPTIONS *opts, BCD_STORE *store)
{
    (void)opts;
    size_t count = BcdStoreGetObjectCount(store);
    for (size_t i = 0; i < count; ++i) {
        BCD_OBJECT *obj = BcdStoreGetObjectAt(store, i);
        if (obj) print_object(obj, opts->verbose);
    }
    return 0;
}

static int parse_object_id(const char *text, BCD_OBJECT_ID *out)
{
    int status = BcdParseObjectId(text, out);
    if (status != BCD_OK) fprintf(stderr, "Invalid object identifier: %s\n", text);
    return status;
}

static int cmd_createstore(const OPTIONS *opts)
{
    BCD_STORE store;
    BcdStoreInit(&store);
    int status = save_bcd_store(opts->pathArg, &store);
    if (status != BCD_OK) fprintf(stderr, "Failed to create store file\n");
    return status;
}

static int cmd_export(const OPTIONS *opts, BCD_STORE *store)
{
    int status = save_bcd_store(opts->pathArg, store);
    if (status != BCD_OK) fprintf(stderr, "Export failed\n");
    return status;
}

static int cmd_import(const OPTIONS *opts)
{
    if (!opts->storePath) {
        const char *sys = resolve_system_store();
        if (!sys) {
            fprintf(stderr, "System store import not supported on this platform\n");
            return BCD_ERR_INVALID_ARG;
        }
        opts = opts; /* silence warning */
    }
    unsigned char *buffer = NULL;
    size_t size = 0;
    if (read_file(opts->pathArg, &buffer, &size) != BCD_OK) {
        fprintf(stderr, "Failed to read import file\n");
        return BCD_ERR_IO;
    }
    const char *target = opts->storePath ? opts->storePath : resolve_system_store();
    int status = write_file(target, buffer, size);
    free(buffer);
    if (status != BCD_OK) fprintf(stderr, "Failed to write target store\n");
    return status;
}

static int element_from_values(const BCD_ELEMENT_META *meta, const OPTIONS *opts, BCD_ELEMENT *el)
{
    if (!meta || !opts || !el) return BCD_ERR_INVALID_ARG;
    memset(el, 0, sizeof(*el));
    el->type = meta->id;
    el->kind = meta->kind;
    if (meta->kind == BCD_ELEMENT_STRING) {
        if (opts->extraCount < 1) return BCD_ERR_INVALID_ARG;
        strncpy(el->data.stringValue, opts->extraValues[0], sizeof(el->data.stringValue) - 1);
    } else if (meta->kind == BCD_ELEMENT_INTEGER) {
        if (opts->extraCount < 1) return BCD_ERR_INVALID_ARG;
        el->data.integerValue = (uint64_t)strtoull(opts->extraValues[0], NULL, 0);
    } else if (meta->kind == BCD_ELEMENT_BOOLEAN) {
        if (opts->extraCount < 1) return BCD_ERR_INVALID_ARG;
        el->data.boolValue = (strcmp(opts->extraValues[0], "ON") == 0 || strcmp(opts->extraValues[0], "on") == 0);
    } else if (meta->kind == BCD_ELEMENT_BINARY) {
        if (opts->extraCount < 1) return BCD_ERR_INVALID_ARG;
        size_t offset = 0;
        for (int i = 0; i < opts->extraCount && offset + sizeof(BCD_OBJECT_ID) <= BCD_MAX_BINARY_SIZE; ++i) {
            BCD_OBJECT_ID id;
            if (parse_object_id(opts->extraValues[i], &id) == BCD_OK) {
                memcpy(el->data.binaryValue.data + offset, &id, sizeof(id));
                offset += sizeof(id);
            }
        }
        el->data.binaryValue.size = offset;
    }
    return BCD_OK;
}

static int cmd_set(const OPTIONS *opts, BCD_STORE *store)
{
    const BCD_ELEMENT_META *meta = BcdLookupElementByName(opts->elementName);
    if (!meta) {
        fprintf(stderr, "Unknown element name: %s\n", opts->elementName);
        return BCD_ERR_INVALID_ARG;
    }
    BCD_OBJECT_ID id;
    if (parse_object_id(opts->idText, &id) != BCD_OK) return BCD_ERR_INVALID_ARG;
    BCD_OBJECT *obj = BcdStoreFindObjectById(store, &id);
    if (!obj) {
        fprintf(stderr, "Object not found\n");
        return BCD_ERR_NOT_FOUND;
    }
    BCD_ELEMENT el;
    if (element_from_values(meta, opts, &el) != BCD_OK) return BCD_ERR_INVALID_ARG;
    int status = BcdObjectSetElement(obj, &el);
    if (status != BCD_OK) fprintf(stderr, "Failed to set element\n");
    return status;
}

static int cmd_deletevalue(const OPTIONS *opts, BCD_STORE *store)
{
    const BCD_ELEMENT_META *meta = BcdLookupElementByName(opts->elementName);
    if (!meta) return BCD_ERR_INVALID_ARG;
    BCD_OBJECT_ID id;
    if (parse_object_id(opts->idText, &id) != BCD_OK) return BCD_ERR_INVALID_ARG;
    BCD_OBJECT *obj = BcdStoreFindObjectById(store, &id);
    if (!obj) return BCD_ERR_NOT_FOUND;
    return BcdObjectRemoveElement(obj, meta->id);
}

static int cmd_delete(const OPTIONS *opts, BCD_STORE *store)
{
    BCD_OBJECT_ID id;
    if (parse_object_id(opts->idText, &id) != BCD_OK) return BCD_ERR_INVALID_ARG;
    return BcdStoreDeleteObject(store, &id);
}

static uint32_t application_type(const char *name)
{
    if (!name) return 0;
    if (strcmp(name, "osloader") == 0) return BCD_OBJECT_OSLOADER;
    if (strcmp(name, "bootmgr") == 0) return BCD_OBJECT_BOOTMGR;
    if (strcmp(name, "resume") == 0) return BCD_OBJECT_RESUME;
    return 0;
}

static int cmd_create(const OPTIONS *opts, BCD_STORE *store)
{
    BCD_OBJECT obj;
    memset(&obj, 0, sizeof(obj));
    if (opts->idText[0]) {
        if (parse_object_id(opts->idText, &obj.id) != BCD_OK) return BCD_ERR_INVALID_ARG;
    } else {
        BcdGenerateObjectId(&obj.id);
    }
    obj.objectType = application_type(opts->application);
    if (opts->description) {
        BCD_ELEMENT el;
        memset(&el, 0, sizeof(el));
        el.type = BCD_ELEMENT_DESCRIPTION;
        el.kind = BCD_ELEMENT_STRING;
        strncpy(el.data.stringValue, opts->description, sizeof(el.data.stringValue) - 1);
        BcdObjectAddElement(&obj, &el);
    }
    int status = BcdStoreAddObject(store, &obj);
    if (status == BCD_OK) {
        char idText[64];
        BcdFormatObjectId(&obj.id, idText, sizeof(idText));
        printf("%s\n", idText);
    }
    return status;
}

static int cmd_copy(const OPTIONS *opts, BCD_STORE *store)
{
    BCD_OBJECT_ID sourceId;
    if (parse_object_id(opts->idText, &sourceId) != BCD_OK) return BCD_ERR_INVALID_ARG;
    BCD_OBJECT *src = BcdStoreFindObjectById(store, &sourceId);
    if (!src) return BCD_ERR_NOT_FOUND;
    BCD_OBJECT copy = *src;
    BcdGenerateObjectId(&copy.id);
    if (opts->description) {
        BCD_ELEMENT *desc = BcdObjectFindElement(&copy, BCD_ELEMENT_DESCRIPTION);
        if (!desc) {
            BCD_ELEMENT el;
            memset(&el, 0, sizeof(el));
            el.type = BCD_ELEMENT_DESCRIPTION;
            el.kind = BCD_ELEMENT_STRING;
            strncpy(el.data.stringValue, opts->description, sizeof(el.data.stringValue) - 1);
            BcdObjectAddElement(&copy, &el);
        } else {
            strncpy(desc->data.stringValue, opts->description, sizeof(desc->data.stringValue) - 1);
        }
    }
    int status = BcdStoreAddObject(store, &copy);
    if (status == BCD_OK) {
        char idText[64];
        BcdFormatObjectId(&copy.id, idText, sizeof(idText));
        printf("%s\n", idText);
    }
    return status;
}

static int cmd_default(const OPTIONS *opts, BCD_STORE *store)
{
    const char *bootmgrIdText = "{9dea862c-5cdd-4e70-acc1-f32b344d4795}";
    BCD_OBJECT_ID bootmgrId;
    if (parse_object_id(bootmgrIdText, &bootmgrId) != BCD_OK) return BCD_ERR_INVALID_ARG;
    BCD_OBJECT *bm = BcdStoreFindObjectById(store, &bootmgrId);
    if (!bm) {
        BCD_OBJECT obj;
        memset(&obj, 0, sizeof(obj));
        obj.id = bootmgrId;
        obj.objectType = BCD_OBJECT_BOOTMGR;
        BcdStoreAddObject(store, &obj);
        bm = BcdStoreFindObjectById(store, &bootmgrId);
    }
    BCD_ELEMENT el;
    memset(&el, 0, sizeof(el));
    el.type = BCD_ELEMENT_BOOTMANAGER_DEFAULT;
    el.kind = BCD_ELEMENT_BINARY;
    el.data.binaryValue.size = sizeof(BCD_OBJECT_ID);
    parse_object_id(opts->targetIdText, (BCD_OBJECT_ID *)el.data.binaryValue.data);
    return BcdObjectSetElement(bm, &el);
}

static int cmd_timeout(const OPTIONS *opts, BCD_STORE *store)
{
    const char *bootmgrIdText = "{9dea862c-5cdd-4e70-acc1-f32b344d4795}";
    BCD_OBJECT_ID bootmgrId;
    if (parse_object_id(bootmgrIdText, &bootmgrId) != BCD_OK) return BCD_ERR_INVALID_ARG;
    BCD_OBJECT *bm = BcdStoreFindObjectById(store, &bootmgrId);
    if (!bm) return BCD_ERR_NOT_FOUND;
    BCD_ELEMENT el;
    memset(&el, 0, sizeof(el));
    el.type = BCD_ELEMENT_TIMEOUT;
    el.kind = BCD_ELEMENT_INTEGER;
    el.data.integerValue = (uint64_t)strtoull(opts->elementName, NULL, 10);
    return BcdObjectSetElement(bm, &el);
}

static int set_order_list(BCD_STORE *store, const OPTIONS *opts, uint32_t elementId)
{
    if (opts->extraCount <= 0) return BCD_ERR_INVALID_ARG;
    BCD_ELEMENT el;
    memset(&el, 0, sizeof(el));
    el.type = elementId;
    el.kind = BCD_ELEMENT_BINARY;
    size_t offset = 0;
    for (int i = 0; i < opts->extraCount && offset + sizeof(BCD_OBJECT_ID) <= BCD_MAX_BINARY_SIZE; ++i) {
        BCD_OBJECT_ID id;
        if (parse_object_id(opts->extraValues[i], &id) == BCD_OK) {
            memcpy(el.data.binaryValue.data + offset, &id, sizeof(id));
            offset += sizeof(id);
        }
    }
    el.data.binaryValue.size = offset;

    const char *bootmgrIdText = "{9dea862c-5cdd-4e70-acc1-f32b344d4795}";
    BCD_OBJECT_ID bootmgrId;
    if (parse_object_id(bootmgrIdText, &bootmgrId) != BCD_OK) return BCD_ERR_INVALID_ARG;
    BCD_OBJECT *bm = BcdStoreFindObjectById(store, &bootmgrId);
    if (!bm) return BCD_ERR_NOT_FOUND;
    return BcdObjectSetElement(bm, &el);
}

int main(int argc, char **argv)
{
    OPTIONS opts;
    if (argc == 1) {
        memset(&opts, 0, sizeof(opts));
        opts.command = CMD_ENUM;
    } else if (parse_options(argc, argv, &opts) != 0) {
        print_usage_summary();
        return 1;
    }

    if (opts.command == CMD_HELP) {
        print_usage_summary();
        return 0;
    }

    const char *storePath = opts.storePath ? opts.storePath : resolve_system_store();
    if (!storePath && (opts.command != CMD_CREATESTORE && opts.command != CMD_IMPORT)) {
        fprintf(stderr, "System store access is not available. Use /store <path>.\n");
        return 1;
    }

    if (opts.command == CMD_CREATESTORE) {
        return cmd_createstore(&opts) == BCD_OK ? 0 : 1;
    }

    if (opts.command == CMD_IMPORT) {
        return cmd_import(&opts) == BCD_OK ? 0 : 1;
    }

    BCD_STORE store;
    if (load_bcd_store(storePath, &store) != BCD_OK) return 1;

    int result = 0;
    switch (opts.command) {
    case CMD_ENUM:
        result = cmd_enum(&opts, &store);
        break;
    case CMD_EXPORT:
        result = cmd_export(&opts, &store);
        break;
    case CMD_CREATE:
        result = cmd_create(&opts, &store);
        break;
    case CMD_COPY:
        result = cmd_copy(&opts, &store);
        break;
    case CMD_DELETE:
        result = cmd_delete(&opts, &store);
        break;
    case CMD_SET:
        result = cmd_set(&opts, &store);
        break;
    case CMD_DELETEVALUE:
        result = cmd_deletevalue(&opts, &store);
        break;
    case CMD_DEFAULT:
        result = cmd_default(&opts, &store);
        break;
    case CMD_TIMEOUT:
        result = cmd_timeout(&opts, &store);
        break;
    case CMD_DISPLAYORDER:
        result = set_order_list(&store, &opts, BCD_ELEMENT_DISPLAY_ORDER);
        break;
    case CMD_BOOTSEQUENCE:
        result = set_order_list(&store, &opts, BCD_ELEMENT_BOOT_SEQUENCE);
        break;
    case CMD_TOOLSDISPLAYORDER:
        result = set_order_list(&store, &opts, BCD_ELEMENT_TOOLS_DISPLAY_ORDER);
        break;
    default:
        result = 0;
        break;
    }

    if (result == BCD_OK && opts.command != CMD_ENUM && opts.command != CMD_EXPORT) {
        if (save_bcd_store(storePath, &store) != BCD_OK) {
            fprintf(stderr, "Failed to write store\n");
            result = 1;
        }
    }

    return result == BCD_OK ? 0 : 1;
}
