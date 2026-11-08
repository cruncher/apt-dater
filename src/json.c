/* apt-dater - terminal-based remote package update manager
 *
 * Authors:
 *   Thomas Liske <liske@ibh.de>
 *
 * Copyright Holder:
 *   2008-2015 (C) IBH IT-Service GmbH [https://www.ibh.de/apt-dater/]
 *
 * License:
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this package; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "apt-dater.h"
#include "json.h"

static const gchar *jsonCategoryName(const Category category) {
 switch(category) {
 case C_UPDATES_PENDING:
  return "updates_pending";
 case C_UP_TO_DATE:
  return "up_to_date";
 case C_BROKEN_PKGS:
  return "broken_packages";
 case C_REFRESH_REQUIRED:
  return "refresh_required";
 case C_REFRESH:
  return "in_refresh";
 case C_SESSIONS:
  return "sessions";
 case C_UNKNOWN:
 default:
  return "unknown";
 }
}

static void jsonAddString(GString *out, const gchar *str) {
 const guchar *p = (const guchar *)str;

 g_string_append_c(out, '"');

 if(str) {
  for(; *p; p++) {
   switch(*p) {
   case '"':
    g_string_append(out, "\\\"");
    break;
   case '\\':
    g_string_append(out, "\\\\");
    break;
   case '\b':
    g_string_append(out, "\\b");
    break;
   case '\f':
    g_string_append(out, "\\f");
    break;
   case '\n':
    g_string_append(out, "\\n");
    break;
   case '\r':
    g_string_append(out, "\\r");
    break;
   case '\t':
    g_string_append(out, "\\t");
    break;
   default:
    if(*p < 0x20)
     g_string_append_printf(out, "\\u%04x", *p);
    else
     g_string_append_c(out, *p);
   }
  }
 }

 g_string_append_c(out, '"');
}

static void jsonPrintReport(GList *hosts) {
 GString *out;
 GList *ho;
 guint total = 0;
 guint up_to_date = 0;
 guint updates_pending = 0;
 guint broken_packages = 0;
 guint refresh_required = 0;
 guint in_refresh = 0;
 guint sessions = 0;
 guint unknown = 0;
 guint i = 0;

 ho = g_list_first(hosts);
 while(ho) {
  HostNode *n = (HostNode *)ho->data;

  total++;

  switch(n->category) {
  case C_UPDATES_PENDING:
   updates_pending++;
   break;
  case C_UP_TO_DATE:
   up_to_date++;
   break;
  case C_BROKEN_PKGS:
   broken_packages++;
   break;
  case C_REFRESH_REQUIRED:
   refresh_required++;
   break;
  case C_REFRESH:
   in_refresh++;
   break;
  case C_SESSIONS:
   sessions++;
   break;
  case C_UNKNOWN:
  default:
   unknown++;
   break;
  }

  ho = g_list_next(ho);
 }

 out = g_string_new(NULL);

 g_string_append_printf(out,
  "{\n"
  "  \"timestamp\": %ld,\n"
  "  \"total\": %u,\n"
  "  \"up_to_date\": %u,\n"
  "  \"updates_pending\": %u,\n"
  "  \"broken_packages\": %u,\n"
  "  \"unknown\": %u,\n"
  "  \"sessions\": %u,\n"
  "  \"refresh_required\": %u,\n"
  "  \"in_refresh\": %u,\n"
  "  \"hosts\": [",
  (long)time(NULL), total, up_to_date, updates_pending, broken_packages,
  unknown, sessions, refresh_required, in_refresh);

 ho = g_list_first(hosts);
 while(ho) {
  HostNode *n = (HostNode *)ho->data;

  if(i++ > 0)
   g_string_append(out, ",");

  g_string_append(out, "\n    {\n      \"hostname\": ");
  jsonAddString(out, n->hostname);
  g_string_append(out, ",\n      \"group\": ");
  jsonAddString(out, n->group);
  g_string_append(out, ",\n      \"status\": ");
  jsonAddString(out, jsonCategoryName(n->category));
  g_string_append_printf(out,
   ",\n      \"updates\": %d,"
   "\n      \"kept_back\": %d,"
   "\n      \"extras\": %d,"
   "\n      \"broken\": %d,"
   "\n      \"reboot_required\": %s,"
   "\n      \"forbid\": %d"
   "\n    }",
   n->nupdates, n->nholds, n->nextras, n->nbrokens,
   (n->status & (HOST_STATUS_KERNELABIUPGR | HOST_STATUS_KERNELVERUPGR)) ? "true" : "false",
   n->forbid);

  ho = g_list_next(ho);
 }

 g_string_append(out, "\n  ]\n}\n");

 fputs(out->str, stdout);
 fflush(stdout);

 g_string_free(out, TRUE);
}

void initJSON(GList *hosts) {
 /* This mode must never touch the terminal. */
 cfg->beep = FALSE;
 cfg->flash = FALSE;
#ifdef FEAT_AUTOREF
 cfg->auto_refresh = FALSE;
#endif

 if(!hosts) return;

 fprintf(stderr, _("apt-dater is refreshing %d hosts, please standby...\n"), g_list_length(hosts));

 GList *n = hosts;
 while(n) {
  ((HostNode *)n->data)->category = C_REFRESH_REQUIRED;

  n = g_list_next(n);
 }
}

gboolean ctrlJSON(GList *hosts) {
 gint torefresh = 0;
 GList *n = hosts;

 while(n) {
  Category category = ((HostNode *)n->data)->category;

  if((category == C_REFRESH_REQUIRED) || (category == C_REFRESH))
   torefresh++;

  n = g_list_next(n);
 }

 if(torefresh > 0)
  return TRUE;

 jsonPrintReport(hosts);

 g_main_loop_quit(loop);

 return FALSE;
}
