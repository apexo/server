/*
Copyright (c) 1998-2015, Enno Rehling <enno@eressea.de>
Katja Zedel <katze@felidae.kn-bremen.de
Christian Schlittchen <corwin@amber.kn-bremen.de>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
**/

#include <platform.h>
#include <kernel/config.h>
#include "hate.h"

#include <kernel/unit.h>
#include <kernel/version.h>

#include <util/attrib.h>
#include <util/gamedata.h>
#include <util/resolve.h>

#include <storage.h>

static int verify_hate(attrib * a, void *owner)
{
    unused_arg(owner);
    if (a->data.v == NULL) {
        return 0;
    }
    return 1;
}

static void
write_hate(const attrib * a, const void *owner, struct storage *store)
{
    write_unit_reference((unit *)a->data.v, store);
}

static int read_hate(attrib * a, void *owner, gamedata *data)
{
    int result = read_reference(&a->data.v, data, read_unit_reference,
        resolve_unit);
    if (result == 0 && !a->data.v) {
        return AT_READ_FAIL;
    }
    return AT_READ_OK;
}

attrib_type at_hate = {
    "hates",
    NULL,
    NULL,
    verify_hate,
    write_hate,
    read_hate,
};

attrib *make_hate(struct unit * u)
{
    attrib *a = a_new(&at_hate);
    a->data.v = u;
    return a;
}
