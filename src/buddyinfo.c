/**
 * collectd - src/buddyinfo.c
 * Copyright (C) 2005-2010  Florian octo Forster
 * Copyright (C) 2009       Manuel Sanmartin
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; only version 2 of the License is applicable.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Authors:
 *   Asaf Kahlon <asafka7 at gmail.com>
 **/

#include "collectd.h"

#include "plugin.h"
#include "utils/common/common.h"
#include "utils/ignorelist/ignorelist.h"

#if !KERNEL_LINUX
#error "No applicable input method."
#endif

#include <unistd.h>

#define MAX_ORDER 11
#define BUDDYINFO_FIELDS MAX_ORDER + 2 // "zone" + Name + (MAX_ORDER entries)
#define NUM_OF_KB(pagesize, order) ((pagesize) / 1024) * (1 << (order))

static const char *config_keys[] = {"Zone"};
static int config_keys_num = STATIC_ARRAY_SIZE(config_keys);

static ignorelist_t *ignorelist;

static int buddyinfo_config(const char *key, const char *value) {
  if (ignorelist == NULL)
    ignorelist = ignorelist_create(1);

  if (strcasecmp(key, "Zone") == 0)
    ignorelist_add(ignorelist, value);
  else
    return -1;

  return 0;
}

static void buddyinfo_submit(const char *zone, const char *size,
                             const int freepages) {
  value_list_t vl = VALUE_LIST_INIT;
  value_t value = {.gauge = freepages};

  if (ignorelist_match(ignorelist, zone) != 0)
    return;

  vl.values = &value;
  vl.values_len = 1;
  sstrncpy(vl.plugin, "buddyinfo", sizeof(vl.plugin));
  sstrncpy(vl.plugin_instance, zone, sizeof(vl.plugin_instance));
  sstrncpy(vl.type, "freepages", sizeof(vl.type));
  sstrncpy(vl.type_instance, size, sizeof(vl.type_instance));

  plugin_dispatch_values(&vl);
}

static int buddyinfo_read(void) {
  FILE *fh;
  char buffer[1024], pagesize_kb[8];
  char *dummy, *zone;
  char *fields[BUDDYINFO_FIELDS];
  int numfields, pagesize = getpagesize();

  if ((fh = fopen("/proc/buddyinfo", "r")) == NULL) {
    WARNING("buddyinfo plugin: fopen: %s", STRERRNO);
    return -1;
  }

  while (fgets(buffer, sizeof(buffer), fh) != NULL) {
    if (!(dummy = strstr(buffer, "zone")))
      continue;

    numfields = strsplit(dummy, fields, BUDDYINFO_FIELDS);
    if (numfields != BUDDYINFO_FIELDS)
      continue;

    zone = fields[1];
    for (int i = 1; i <= MAX_ORDER; i++) {
      ssnprintf(pagesize_kb, sizeof(pagesize_kb), "%dKB",
                NUM_OF_KB(pagesize, i - 1));
      buddyinfo_submit(zone, pagesize_kb, atoll(fields[i + 1]));
    }
  }

  fclose(fh);
  return 0;
}

void module_register(void) {
  plugin_register_config("buddyinfo", buddyinfo_config, config_keys,
                         config_keys_num);
  plugin_register_read("buddyinfo", buddyinfo_read);
}
