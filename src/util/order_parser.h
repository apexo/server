#pragma once
/* 
 * +-------------------+  Christian Schlittchen <corwin@amber.kn-bremen.de>
 * |                   |  Enno Rehling <enno@eressea.de>
 * | Eressea PBEM host |  Katja Zedel <katze@felidae.kn-bremen.de>
 * | (c) 1998 - 2007   |
 * |                   |  This program may not be used, modified or distributed
 * +-------------------+  without prior permission by the authors of Eressea.
 *
 */

#ifndef UTIL_ORDER_PARSER_H
#define UTIL_ORDER_PARSER_H

#include <stddef.h>
#include <stdbool.h>

struct OrderParserStruct;
typedef struct OrderParserStruct *OP_Parser;

enum OP_Status {
    OP_STATUS_ERROR = 0,
    OP_STATUS_OK = 1
};

typedef void(*OP_FactionHandler) (void *userData, int no, const char *password);
typedef void(*OP_UnitHandler) (void *userData, int no);
typedef void(*OP_OrderHandler) (void *userData, const char *str);

OP_Parser OP_ParserCreate(void);
void OP_ParserFree(OP_Parser op);
enum OP_Status OP_Parse(OP_Parser op, const char *s, int len, int isFinal);
void OP_SetUnitHandler(OP_Parser op, OP_UnitHandler handler);
void OP_SetFactionHandler(OP_Parser op, OP_FactionHandler handler);
void OP_SetOrderHandler(OP_Parser op, OP_OrderHandler handler);
void OP_SetUserData(OP_Parser op, void *userData);

#endif
