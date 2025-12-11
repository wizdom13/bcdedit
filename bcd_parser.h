#ifndef BCD_PARSER_H
#define BCD_PARSER_H

#include "bcd.h"
#include "regf.h"

int BcdStoreLoadFromHive(BCD_STORE *store, REGF_HIVE *hive);
int BcdStoreSerializeToHive(const BCD_STORE *store, unsigned char **outBuffer, size_t *outSize);

#endif /* BCD_PARSER_H */
