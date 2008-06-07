/* vi: set ts=2:
 *
 *
 *	Eressea PB(E)M host Copyright (C) 1998-2003
 *      Christian Schlittchen (corwin@amber.kn-bremen.de)
 *      Katja Zedel (katze@felidae.kn-bremen.de)
 *      Henning Peters (faroul@beyond.kn-bremen.de)
 *      Enno Rehling (enno@eressea.de)
 *      Ingo Wilken (Ingo.Wilken@informatik.uni-oldenburg.de)
 *
 * This program may not be used, modified or distributed without
 * prior permission by the authors of Eressea.
 */

#include <config.h>
#include <kernel/eressea.h>
#include "xmlreport.h"

/* tweakable features */
#define ENCODE_SPECIAL 1
#define RENDER_CRMESSAGES

#define XML_ATL_NAMESPACE (const xmlChar *) "http://www.eressea.de/XML/2008/atlantis"
#define XML_XML_LANG (const xmlChar *) "lang"

/* modules include */
#include <modules/score.h>

/* attributes include */
#include <attributes/follow.h>
#include <attributes/racename.h>
#include <attributes/orcification.h>
#include <attributes/otherfaction.h>
#include <attributes/raceprefix.h>

/* gamecode includes */
#include "laws.h"
#include "economy.h"

/* kernel includes */
#include <kernel/alchemy.h>
#include <kernel/alliance.h>
#include <kernel/border.h>
#include <kernel/building.h>
#include <kernel/faction.h>
#include <kernel/group.h>
#include <kernel/item.h>
#include <kernel/karma.h>
#include <kernel/magic.h>
#include <kernel/message.h>
#include <kernel/move.h>
#include <kernel/order.h>
#include <kernel/plane.h>
#include <kernel/race.h>
#include <kernel/region.h>
#include <kernel/reports.h>
#include <kernel/resources.h>
#include <kernel/ship.h>
#include <kernel/skill.h>
#include <kernel/teleport.h>
#include <kernel/terrain.h>
#include <kernel/unit.h>
#include <kernel/save.h>

/* util includes */
#include <util/attrib.h>
#include <util/base36.h>
#include <util/goodies.h>
#include <util/language.h>
#include <util/message.h>
#include <util/unicode.h>

/* libxml2 includes */
#include <libxml/tree.h>
#include <libxml/encoding.h>
#ifdef USE_ICONV
#include <iconv.h>
#endif

/* libc includes */
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define L10N(x) x

typedef struct xml_context {
  xmlDocPtr doc;
  xmlNsPtr ns_atl;
  xmlNsPtr ns_xml;
} xml_context;

static const xmlChar *
xml_s(const char * str)
{
  static xmlChar buffer[1024];
  const char * inbuf = str;
  char * outbuf = (char *)buffer;
  size_t inbytes = strlen(str)+1;
  size_t outbytes = sizeof(buffer) - 1;

  unicode_latin1_to_utf8(outbuf, &outbytes, inbuf, &inbytes);
  buffer[outbytes] = 0;
  return buffer;
}

static const xmlChar *
xml_i(double number)
{
  static char buffer[128];
  snprintf(buffer, sizeof(buffer), "%.0lf", number);
  return (const xmlChar *)buffer;
}

static xmlNodePtr
xml_link(report_context * ctx, const xmlChar * rel, const xmlChar * ref)
{
  xml_context* xct = (xml_context*)ctx->userdata;
  xmlNodePtr node = xmlNewNode(xct->ns_atl, BAD_CAST "link");

  xmlNewNsProp(node, xct->ns_atl, BAD_CAST "rel", rel);
  xmlNewNsProp(node, xct->ns_atl, BAD_CAST "ref", ref);

  return node;
}

static const xmlChar *
xml_ref_unit(const unit * u)
{
  static char idbuf[20];
  snprintf(idbuf, sizeof(idbuf), "unit_%d", u->no);
  return (const xmlChar *)idbuf;
}

static const xmlChar *
xml_ref_faction(const faction * f)
{
  static char idbuf[20];
  snprintf(idbuf, sizeof(idbuf), "fctn_%d", f->no);
  return (const xmlChar *)idbuf;
}

static const xmlChar *
xml_ref_group(const group * g)
{
  static char idbuf[20];
  snprintf(idbuf, sizeof(idbuf), "grp_%d", g->gid);
  return (const xmlChar *)idbuf;
}

static const xmlChar *
xml_ref_prefix(const char * str)
{
  static char idbuf[20];
  snprintf(idbuf, sizeof(idbuf), "grp_%s", str);
  return (const xmlChar *)idbuf;
}

static const xmlChar *
xml_ref_building(const building * b)
{
  static char idbuf[20];
  snprintf(idbuf, sizeof(idbuf), "bldg_%d", b->no);
  return (const xmlChar *)idbuf;
}

static const xmlChar *
xml_ref_ship(const ship * sh)
{
  static char idbuf[20];
  snprintf(idbuf, sizeof(idbuf), "shp_%d", sh->no);
  return (const xmlChar *)idbuf;
}

static const xmlChar *
xml_ref_region(const region * r)
{
  static char idbuf[20];
  snprintf(idbuf, sizeof(idbuf), "rgn_%d", r->uid);
  return (const xmlChar *)idbuf;
}

static xmlNodePtr
xml_inventory(report_context * ctx, item * items, unit * u)
{
  xml_context* xct = (xml_context*)ctx->userdata;
  xmlNodePtr node = xmlNewNode(xct->ns_atl, BAD_CAST "items");
  item * itm;

  for (itm=items;itm;itm=itm->next) {
    xmlNodePtr child;
    const char * name;
    int n;

    child = xmlNewNode(xct->ns_atl, BAD_CAST "item");
    xmlAddChild(node, child);
    report_item(u, itm, ctx->f, NULL, &name, &n, true);
    xmlNewNsProp(child, xct->ns_atl, BAD_CAST "type", (xmlChar *)name);
    xmlNodeAddContent(child, (xmlChar*)itoab(n, 10));
  }
  return node;
}

static xmlNodePtr
xml_unit(report_context * ctx, unit * u, int mode)
{
  xml_context* xct = (xml_context*)ctx->userdata;
  xmlNodePtr node = xmlNewNode(xct->ns_atl, BAD_CAST "unit");
  static const curse_type * itemcloak_ct = 0;
  static boolean init = false;
  xmlNodePtr child;
  const char * str, * rcname, * rcillusion;
  boolean disclosure = (ctx->f == u->faction || omniscient(ctx->f));

  xmlNewNsProp(node, xct->ns_xml, XML_XML_ID, xml_ref_unit(u));
  xmlNewNsProp(node, xct->ns_atl, BAD_CAST "key", BAD_CAST itoa36(u->no));
  xmlNewTextChild(node, xct->ns_atl, BAD_CAST "name", (const xmlChar *)u->name);
  xmlNewTextChild(node, xct->ns_atl, BAD_CAST "number", (const xmlChar *)itoab(u->number, 10));

  /* optional description */
  str = u_description(u, ctx->f->locale);
  if (str) {
    child = xmlNewTextChild(node, xct->ns_atl, BAD_CAST "text", (const xmlChar *)str);
    xmlNewNsProp(child, xct->ns_atl, BAD_CAST "rel", BAD_CAST "public");
    if (str!=u->display) {
      xmlNewNsProp(child, xct->ns_atl, XML_XML_LANG, BAD_CAST locale_name(ctx->f->locale));
    }
  }

  /* possible <guard/> info */
  if (getguard(u)) {
    xmlAddChild(node, xmlNewNode(xct->ns_atl, BAD_CAST "guard"));
  }

  /* siege */
  if (fval(u, UFL_SIEGE)) {
    building * b = usiege(u);
    if (b) {
      xmlAddChild(node, xml_link(ctx, BAD_CAST "siege", xml_ref_building(b)));
    }
  }

  /* TODO: temp/alias */

  /* race information */
  report_race(u, &rcname, &rcillusion);
  if (disclosure) {
    child = xmlNewNode(xct->ns_atl, BAD_CAST "race");
    xmlAddChild(node, child);
    xmlNewNsProp(child, xct->ns_atl, BAD_CAST "rel", BAD_CAST "true");
    xmlNewNsProp(child, xct->ns_atl, BAD_CAST "ref", (const xmlChar *)rcname);
    if (rcillusion) {
      child = xmlNewNode(xct->ns_atl, BAD_CAST "race");
      xmlAddChild(node, child);
      xmlNewNsProp(child, xct->ns_atl, BAD_CAST "rel", BAD_CAST "stealth");
      xmlNewNsProp(child, xct->ns_atl, BAD_CAST "ref", (const xmlChar *)rcillusion);
    }
  } else {
    child = xmlNewNode(xct->ns_atl, BAD_CAST "race");
    xmlAddChild(node, child);
    xmlNewNsProp(child, xct->ns_atl, BAD_CAST "ref", (const xmlChar *)(rcillusion?rcillusion:rcname));
  }

  /* group and prefix information. we only write the prefix if we really must */
  if (fval(u, UFL_GROUP)) {
    attrib * a = a_find(u->attribs, &at_group);
    if (a!=NULL) {
      const group * g = (const group*)a->data.v;
      if (disclosure) {
        child = xmlNewNode(xct->ns_atl, BAD_CAST "group");
        xmlAddChild(node, child);
        xmlNewNsProp(child, xct->ns_atl, BAD_CAST "ref", xml_ref_group(g));
      } else {
        const char * prefix = get_prefix(g->attribs);
        child = xmlNewNode(xct->ns_atl, BAD_CAST "prefix");
        xmlAddChild(node, child);
        xmlNewNsProp(child, xct->ns_atl, BAD_CAST "ref", xml_ref_prefix(prefix));
      }
    }
  }

  if (disclosure) {
    unit * mage;
    
    str = uprivate(u);
    if (str) {
      child = xmlNewTextChild(node, xct->ns_atl, BAD_CAST "text", (const xmlChar *)str);
      xmlNewNsProp(child, xct->ns_atl, BAD_CAST "rel", BAD_CAST "private");
    }

    /* familiar info */
    mage = get_familiar_mage(u);
    if (mage) xmlAddChild(node, xml_link(ctx, BAD_CAST "familiar_of", xml_ref_unit(mage)));

    /* combat status */
    child = xmlNewNode(xct->ns_atl, BAD_CAST "status");
    xmlAddChild(node, child);
    xmlSetNsProp(child, xct->ns_atl, BAD_CAST "rel", BAD_CAST "combat");
    xmlSetNsProp(child, xct->ns_atl, BAD_CAST "value", BAD_CAST combatstatus[u->status]);

    if (fval(u, UFL_NOAID)) {
      child = xmlNewNode(xct->ns_atl, BAD_CAST "status");
      xmlAddChild(node, child);
      xmlSetNsProp(child, xct->ns_atl, BAD_CAST "rel", BAD_CAST "aid");
      xmlSetNsProp(child, xct->ns_atl, BAD_CAST "value", BAD_CAST "false");
    }

    if (fval(u, UFL_STEALTH)) {
      int i = u_geteffstealth(u);
      if (i>=0) {
        child = xmlNewNode(xct->ns_atl, BAD_CAST "status");
        xmlAddChild(node, child);
        xmlSetNsProp(child, xct->ns_atl, BAD_CAST "rel", BAD_CAST "stealth");
        xmlSetNsProp(child, xct->ns_atl, BAD_CAST "value", BAD_CAST itoab(i, 10));
      }
    }
    if (fval(u, UFL_HERO)) {
      child = xmlNewNode(xct->ns_atl, BAD_CAST "status");
      xmlAddChild(node, child);
      xmlSetNsProp(child, xct->ns_atl, BAD_CAST "rel", BAD_CAST "hero");
      xmlSetNsProp(child, xct->ns_atl, BAD_CAST "value", BAD_CAST "true");
    }

    if (fval(u, UFL_HUNGER)) {
      child = xmlNewNode(xct->ns_atl, BAD_CAST "status");
      xmlAddChild(node, child);
      xmlSetNsProp(child, xct->ns_atl, BAD_CAST "rel", BAD_CAST "hunger");
      xmlSetNsProp(child, xct->ns_atl, BAD_CAST "value", BAD_CAST "true");
    }

    /* TODO: hitpoints, aura */

  }

  /* faction information w/ visibiility */
  child = xmlNewNode(xct->ns_atl, BAD_CAST "faction");
  xmlAddChild(node, child);
  if (disclosure) {
    xmlNewNsProp(child, xct->ns_atl, BAD_CAST "rel", BAD_CAST "true");
    xmlNewNsProp(child, xct->ns_atl, BAD_CAST "ref", xml_ref_faction(u->faction));

    if (fval(u, UFL_PARTEITARNUNG)) {
      const faction * sf = visible_faction(NULL, u);
      child = xmlNewNode(xct->ns_atl, BAD_CAST "faction");
      xmlAddChild(node, child);
      xmlNewNsProp(child, xct->ns_atl, BAD_CAST "rel", BAD_CAST "stealth");
      xmlNewNsProp(child, xct->ns_atl, BAD_CAST "ref", xml_ref_faction(sf));
    }
  } else {
    const faction * sf = visible_faction(ctx->f, u);
    if (sf==ctx->f) {
      xmlNewNsProp(child, xct->ns_atl, BAD_CAST "rel", BAD_CAST "stealth");
    }
    xmlNewNsProp(child, xct->ns_atl, BAD_CAST "ref", xml_ref_faction(sf));
  }

  /* the inventory */
  if (u->items) {
    item result[MAX_INVENTORY];
    item * show = NULL;

    if (!init) {
      init = true;
      itemcloak_ct = ct_find("itemcloak");
    }

    if (disclosure) {
      show = u->items;
    } else {
      boolean see_items = (mode >= see_unit);
      if (see_items) {
        if (itemcloak_ct && !curse_active(get_curse(u->attribs, itemcloak_ct))) {
          see_items = false;
        } else {
          see_items = effskill(u, SK_STEALTH) < 3;
        }
      }
      if (see_items) {
        int n = report_items(u->items, result, MAX_INVENTORY, u, ctx->f);
        assert(n>=0);
        if (n>0) show = result;
        else show = NULL;
      } else {
        show = NULL;
      }
    }

    if (show) {
      xmlAddChild(node, xml_inventory(ctx, show, u));
    }
  }

  return node;
}

static xmlNodePtr
xml_resources(report_context * ctx, const seen_region * sr)
{
  xml_context* xct = (xml_context*)ctx->userdata;
  xmlNodePtr node = xmlNewNode(xct->ns_atl, BAD_CAST "resources");
  resource_report result[MAX_RAWMATERIALS];
  int n, size = report_resources(sr, result, MAX_RAWMATERIALS, ctx->f);

  for (n=0;n<size;++n) {
    if (result[n].number>=0) {
      xmlNodePtr child;
      
      child = xmlNewNode(xct->ns_atl, BAD_CAST "resource");
      xmlAddChild(node, child);
      xmlNewNsProp(child, xct->ns_atl, BAD_CAST "type", (xmlChar*)result[n].name);
      if (result[n].level>=0) {
        xmlNewNsProp(child, xct->ns_atl, BAD_CAST "level", (xmlChar*)itoab(result[n].level, 10));
      }
      xmlNodeAddContent(child, (xmlChar*)itoab(result[n].number, 10));
    }
  }
  return node;
}

static xmlNodePtr
xml_faction(report_context * ctx, faction * f)
{
  xml_context* xct = (xml_context*)ctx->userdata;
  xmlNodePtr node = xmlNewNode(xct->ns_atl, BAD_CAST "faction");

  xmlNewNsProp(node, xct->ns_xml, XML_XML_ID, xml_ref_faction(f));
  xmlNewNsProp(node, xct->ns_atl, BAD_CAST "key", BAD_CAST itoa36(f->no));
  xmlNewTextChild(node, xct->ns_atl, BAD_CAST "name", (const xmlChar *)f->name);

  if (ctx->f==f) {
    xmlAddChild(node, xml_link(ctx, BAD_CAST "race", BAD_CAST f->race->_name[0]));
    if (f->items) xmlAddChild(node, xml_inventory(ctx, f->items, NULL));
  }
  return node;
}

static xmlNodePtr
xml_building(report_context * ctx, seen_region * sr, const building * b, const unit * owner)
{
  xml_context* xct = (xml_context*)ctx->userdata;
  xmlNodePtr node = xmlNewNode(xct->ns_atl, BAD_CAST "building");
  xmlNodePtr child;
  const char * bname, * billusion;

  xmlNewNsProp(node, xct->ns_xml, XML_XML_ID, xml_ref_building(b));
  xmlNewNsProp(node, xct->ns_atl, BAD_CAST "key", BAD_CAST itoa36(b->no));
  xmlNewTextChild(node, xct->ns_atl, BAD_CAST "name", (const xmlChar *)b->name);
  xmlNewTextChild(node, xct->ns_atl, BAD_CAST "size", (const xmlChar *)itoab(b->size, 10));
  if (b->display && b->display[0]) {
    child = xmlNewTextChild(node, xct->ns_atl, BAD_CAST "text", (const xmlChar *)b->display);
    xmlNewNsProp(child, xct->ns_atl, BAD_CAST "rel", BAD_CAST "public");
  }
  if (b->besieged) {
    xmlNewTextChild(node, xct->ns_atl, BAD_CAST "siege", (const xmlChar *)itoab(b->besieged, 10));
  }
  if (owner) xmlAddChild(node, xml_link(ctx, BAD_CAST "owner", xml_ref_unit(owner)));

  report_building(b, &bname, &billusion);
  if (owner && owner->faction==ctx->f) {
    child = xmlNewNode(xct->ns_atl, BAD_CAST "type");
    xmlAddChild(node, child);
    xmlNewNsProp(child, xct->ns_atl, BAD_CAST "rel", BAD_CAST "true");
    xmlNewNsProp(child, xct->ns_atl, BAD_CAST "ref", (const xmlChar *)bname);
    if (billusion) {
      child = xmlNewNode(xct->ns_atl, BAD_CAST "type");
      xmlAddChild(node, child);
      xmlNewNsProp(child, xct->ns_atl, BAD_CAST "rel", BAD_CAST "illusion");
      xmlNewNsProp(child, xct->ns_atl, BAD_CAST "ref", (const xmlChar *)billusion);
    }
  } else {
    child = xmlNewNode(xct->ns_atl, BAD_CAST "type");
    xmlAddChild(node, child);
    xmlNewNsProp(child, xct->ns_atl, BAD_CAST "ref", (const xmlChar *)(billusion?billusion:bname));
  }

  return node;
}

static xmlNodePtr
xml_ship(report_context * ctx, const seen_region * sr, const ship * sh, const unit * owner)
{
  xml_context* xct = (xml_context*)ctx->userdata;
  xmlNodePtr child, node = xmlNewNode(xct->ns_atl, BAD_CAST "ship");

  xmlNewNsProp(node, xct->ns_xml, XML_XML_ID, xml_ref_ship(sh));
  xmlNewNsProp(node, xct->ns_atl, BAD_CAST "key", BAD_CAST itoa36(sh->no));
  xmlNewTextChild(node, xct->ns_atl, BAD_CAST "name", (const xmlChar *)sh->name);
  xmlNewTextChild(node, xct->ns_atl, BAD_CAST "size", (const xmlChar *)itoab(sh->size, 10));

  if (sh->damage) {
    xmlNewTextChild(node, xct->ns_atl, BAD_CAST "damage", (const xmlChar *)itoab(sh->damage, 10));
  }

  if (fval(sr->r->terrain, SEA_REGION) && sh->coast!=NODIRECTION) {
    xmlNewTextChild(node, xct->ns_atl, BAD_CAST "coast", BAD_CAST directions[sh->coast]);
  }

  child = xmlNewNode(xct->ns_atl, BAD_CAST "type");
  xmlAddChild(node, child);
  xmlNewNsProp(child, xct->ns_atl, BAD_CAST "ref", (const xmlChar *)sh->type->name[0]);

  if (sh->display && sh->display[0]) {
    child = xmlNewTextChild(node, xct->ns_atl, BAD_CAST "text", (const xmlChar *)sh->display);
    xmlNewNsProp(child, xct->ns_atl, BAD_CAST "rel", BAD_CAST "public");
  }

  if (owner) xmlAddChild(node, xml_link(ctx, BAD_CAST "owner", xml_ref_unit(owner)));

  if ((owner && owner->faction == ctx->f) || omniscient(ctx->f)) {
    int n = 0, p = 0;
    getshipweight(sh, &n, &p);
    xmlNewTextChild(node, xct->ns_atl, BAD_CAST "cargo", (const xmlChar *)itoab(n, 10));
  }
  return node;
}

static xmlNodePtr
xml_region(report_context * ctx, seen_region * sr)
{
  xml_context* xct = (xml_context*)ctx->userdata;
  const region * r = sr->r;
  xmlNodePtr node = xmlNewNode(xct->ns_atl, BAD_CAST "region");
  xmlNodePtr child;
  int stealthmod = stealth_modifier(sr->mode);
  unit * u;
  ship * sh = r->ships;
  building * b = r->buildings;

  xmlNewNsProp(node, xct->ns_xml, XML_XML_ID, xml_ref_region(r));

  child = xmlNewNode(xct->ns_atl, BAD_CAST "coordinate");
  xmlAddChild(node, child);
  xmlNewNsProp(child, xct->ns_atl, BAD_CAST "x", xml_i(region_x(r, ctx->f)));
  xmlNewNsProp(child, xct->ns_atl, BAD_CAST "y", xml_i(region_y(r, ctx->f)));
  if (r->planep) {
    xmlNewNsProp(child, xct->ns_atl, BAD_CAST "plane", xml_s(r->planep->name));
  }

  child = xmlNewNode(xct->ns_atl, BAD_CAST "terrain");
  xmlAddChild(node, child);
  xmlNewNsProp(child, xct->ns_atl, BAD_CAST "ref", (xmlChar *)terrain_name(r));

  if (r->land!=NULL) {
    child = xmlNewTextChild(node, xct->ns_atl, BAD_CAST "name", (const xmlChar *)r->land->name);
    if (r->land->items) {
      xmlAddChild(node, xml_inventory(ctx, r->land->items, NULL));
    }
  }
  if (r->display && r->display[0]) {
    child = xmlNewTextChild(node, xct->ns_atl, BAD_CAST "text", (const xmlChar *)r->display);
    xmlNewNsProp(child, xct->ns_atl, BAD_CAST "rel", BAD_CAST "public");
  }
  xmlAddChild(node, xml_resources(ctx, sr));

  child = xmlNewNode(xct->ns_atl, BAD_CAST "terrain");
  xmlNewNsProp(child, xct->ns_atl, BAD_CAST "ref", (const xmlChar *)terrain_name(r));

  for (u=r->units;u;u=u->next) {
    if (u->building || u->ship || (stealthmod>INT_MIN && cansee(ctx->f, r, u, stealthmod))) {
      xmlAddChild(node, xml_unit(ctx, u, sr->mode));
    }
  }

  /* report all units. they are pre-sorted in an efficient manner */
  u = r->units;
  while (b) {
    while (b && (!u || u->building!=b)) {
      xmlAddChild(node, xml_building(ctx, sr, b, NULL));
      b = b->next;
    }
    if (b) {
      xmlAddChild(node, xml_building(ctx, sr, b, u));
      while (u && u->building==b) {
        xmlAddChild(child, xml_unit(ctx, u, sr->mode));
        u = u->next;
      }
      b = b->next;
    }
  }
  while (u && !u->ship) {
    if (stealthmod>INT_MIN) {
      if (u->faction == ctx->f || cansee(ctx->f, r, u, stealthmod)) {
        xmlAddChild(node, xml_unit(ctx, u, sr->mode));
      }
    }
    u = u->next;
  }
  while (sh) {
    while (sh && (!u || u->ship!=sh)) {
      xmlAddChild(node, xml_ship(ctx, sr, sh, NULL));
      sh = sh->next;
    }
    if (sh) {
      xmlAddChild(node, xml_ship(ctx, sr, sh, u));
      while (u && u->ship==sh) {
        xmlAddChild(child, xml_unit(ctx, u, sr->mode));
        u = u->next;
      }
      sh = sh->next;
    }
  }
  return node;
}

static xmlNodePtr
report_root(report_context * ctx)
{
  const faction_list * address;
  region * r = ctx->first, * rend = ctx->last;
  xml_context* xct = (xml_context*)ctx->userdata;
  xmlNodePtr xmlReport = xmlNewNode(NULL, BAD_CAST "atlantis");

  xct->ns_xml = xmlNewNs(xmlReport, XML_XML_NAMESPACE, BAD_CAST "xml");
  xct->ns_atl = xmlNewNs(xmlReport, XML_ATL_NAMESPACE, NULL);
  xmlSetNs(xmlReport, xct->ns_atl);

  for (address=ctx->addresses;address;address=address->next) {
    xmlAddChild(xmlReport, xml_faction(ctx, address->data));
  }

  for (;r!=rend;r=r->next) {
    seen_region * sr = find_seen(ctx->seen, r);
    if (sr!=NULL) xmlAddChild(xmlReport, xml_region(ctx, sr));
  }
  return xmlReport;
};

/* main function of the xmlreport. creates the header and traverses all regions */
static int
report_xml(const char * filename, report_context * ctx, const char * encoding)
{
  xml_context xct;
  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");

  xct.doc = doc;
  assert(ctx->userdata==NULL);
  ctx->userdata = &xct;

  xmlDocSetRootElement(doc, report_root(ctx));
  xmlKeepBlanksDefault(0);
  xmlSaveFormatFileEnc(filename, doc, "utf-8", 1);
  xmlFreeDoc(doc);

  ctx->userdata = NULL;

  return 0;
}

void
xmlreport_init(void)
{
  register_reporttype("xml", &report_xml, 1<<O_XML);
#ifdef USE_ICONV
  utf8 = iconv_open("UTF-8", "");
#endif
}

void
xmlreport_cleanup(void)
{
#ifdef USE_ICONV
  iconv_close(utf8);
#endif
}
