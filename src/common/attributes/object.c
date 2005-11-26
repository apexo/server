/* vi: set ts=2:
 *
 * 
 * Eressea PB(E)M host Copyright (C) 1998-2003
 *      Christian Schlittchen (corwin@amber.kn-bremen.de)
 *      Katja Zedel (katze@felidae.kn-bremen.de)
 *      Henning Peters (faroul@beyond.kn-bremen.de)
 *      Enno Rehling (enno@eressea-pbem.de)
 *      Ingo Wilken (Ingo.Wilken@informatik.uni-oldenburg.de)
 *
 * This program may not be used, modified or distributed without
 * prior permission by the authors of Eressea.
 */

#include <config.h>
#include <eressea.h>
#include "object.h"

/* kernel includes */
#include <kernel/unit.h>
#include <kernel/faction.h>
#include <kernel/region.h>
#include <kernel/ship.h>
#include <kernel/building.h>

/* util includes */
#include <util/attrib.h>

/* stdc includes */
#include <string.h>

typedef struct object_data {
  object_type type;
  char * name;
  union {
    int i;
    char * str;
    double real;
    struct unit * u;
    struct region * r;
    struct building * b;
    struct ship * sh;
    struct faction * f;
  } data;
} object_data;

static void
object_write(const attrib *a, FILE *F)
{
  const object_data * data = (object_data *)a->data.v;
  int type = (int)data->type;
  fprintf(F, "%s %d", data->name, type);
  switch (data->type) {
    case TUNIT:
      write_unit_reference(data->data.u, F);
      break;
    case TFACTION:
      write_faction_reference(data->data.f, F);
      break;
    case TBUILDING:
      write_building_reference(data->data.b, F);
      break;
    case TSHIP:
      /* write_ship_reference(data->data.sh, F); */
      assert(!"not implemented");
      break;
    case TREGION:
      write_region_reference(data->data.r, F);
      break;
    case TNONE:
      break;
    default:
      assert(!"illegal type in object-attribute");
  }
}

static int
object_read(attrib *a, FILE *F)
{
  object_data * data = (object_data *)a->data.v;
  int type;
  char name[64];
  fscanf(F, "%s %d", name, &type);
  data->type = (object_type)type;
  switch (data->type) {
    case TUNIT:
      return read_unit_reference(&data->data.u, F);
    case TFACTION:
      return read_faction_reference(&data->data.f, F);
    case TBUILDING:
      return read_building_reference(&data->data.b, F);
    case TSHIP:
      /* return read_ship_reference(&data->data.sh, F); */
      assert(!"not implemented");
      break;
    case TREGION:
      return read_region_reference(&data->data.r, F);
    case TNONE:
      break;
    default:
      return AT_READ_FAIL;
  }
  return AT_READ_OK;
}

static void
object_init(attrib * a)
{
  object_data * data;
  a->data.v = malloc(sizeof(object_data));
  data = (object_data *)a->data.v;
  data->type = TNONE;
}

static void
object_done(attrib * a)
{
  object_data * data = (object_data *)a->data.v;
  if (data->type == TSTRING) free(data->data.str);
  free(a->data.v);
}

attrib_type at_object = {
  "object", object_init, object_done, NULL,
  object_write, object_read
};

const char *
object_name(const attrib * a)
{
  object_data * data = (object_data *)a->data.v;
  return data->name;
}

struct attrib *
object_create(const char * name, object_type type, variant value)
{
  attrib * a = a_new(&at_object);
  object_data * data = (object_data *)a->data.v;

  data->type = type;
  data->name = strdup(name);
  switch (type) {
    case TSTRING:
      data->data.str = strdup(value.v);
      break;
    case TINTEGER:
      data->data.i = value.i;
      break;
    case TREAL:
      data->data.real = value.f;
      break;
    case TREGION:
      data->data.r = (region*)value.v;
      break;
    case TBUILDING:
      data->data.b = (building*)value.v;
      break;
    case TFACTION:
      data->data.f = (faction*)value.v;
      break;
    case TUNIT:
      data->data.u = (unit*)value.v;
      break;
    case TSHIP:
      data->data.sh = (ship*)value.v;
      break;
    case TNONE:
      break;
    default:
      assert(!"invalid object-type");
      break;
  }
  return a;
}

extern void object_get(const struct attrib * a, variant * value, object_type * type);
