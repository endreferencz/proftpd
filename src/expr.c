/*
 * ProFTPD - FTP server daemon
 * Copyright (c) 2008 The ProFTPD Project team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
 *
 * As a special exemption, The ProFTPD Project team and other respective
 * copyright holders give permission to link this program with OpenSSL, and
 * distribute the resulting executable, without including the source code for
 * OpenSSL in the source distribution.
 */

/* Expression API implementation
 * $Id: expr.c,v 1.1 2008-06-05 08:01:39 castaglia Exp $
 */

#include "conf.h"

array_header *pr_expr_create(pool *p, int *argc, char **argv) {
  array_header *acl = NULL;
  int cnt = *argc;
  char *s, *ent;

  if (cnt) {
    acl = make_array(p, cnt, sizeof(char *));

    while (cnt-- && *(++argv)) {
      s = pstrdup(p, *argv);

      while ((ent = pr_str_get_token(&s, ",")) != NULL) {
        if (*ent)
          *((char **) push_array(acl)) = ent;
      }
    }

    *argc = acl->nelts;

  } else
    *argc = 0;

  return acl;
}

/* Boolean "class-expression" AND matching, returns TRUE if the expression
 * evaluates to TRUE.
 */
int pr_expr_eval_class_and(char **expr) {
  int found;
  char *class;

  for (; *expr; expr++) {
    class = *expr;
    found = FALSE;

    if (*class == '!') {
      found = !found;
      class++;
    }

    if (!session.class && !found)
      return FALSE;

    if (session.class && strcmp(session.class->cls_name, class) == 0)
      found = !found;

    if (!found)
      return FALSE;
  }

  return TRUE;
}

/* Boolean "class-expression" OR matching, returns TRUE if the expression
 * evaluates to TRUE.
 */
int pr_expr_eval_class_or(char **expr) {
  int found;
  char *class;

  for (; *expr; expr++) {
    class = *expr;
    found = FALSE;

    if (*class == '!') {
      found = !found;
      class++;
    }

    if (!session.class)
      return found;

    if (strcmp(session.class->cls_name, class) == 0)
      found = !found;

    if (found)
      return TRUE;
  }

  return FALSE;
}

/* Boolean "group-expression" AND matching, returns TRUE if the expression
 * evaluates to TRUE.
 */
int pr_expr_eval_group_and(char **expr) {
  int found;
  char *grp;

  for (; *expr; expr++) {
    grp = *expr;
    found = FALSE;

    if (*grp == '!') {
      found = !found;
      grp++;
    }

    if (session.group && strcmp(session.group, grp) == 0)
      found = !found;

    else if (session.groups) {
      register int i = 0;

      for (i = session.groups->nelts-1; i >= 0; i--)
        if (strcmp(*(((char **) session.groups->elts) + i), grp) == 0) {
          found = !found;
          break;
        }
    }

    if (!found)
      return FALSE;
  }

  return TRUE;
}

/* Boolean "group-expression" OR matching, returns TRUE if the expression
 * evaluates to TRUE.
 */
int pr_expr_eval_group_or(char **expr) {
  int found;
  char *grp;

  for (; *expr; expr++) {
    grp = *expr;
    found = FALSE;

    if (*grp == '!') {
      found = !found;
      grp++;
    }

    if (session.group && strcmp(session.group, grp) == 0)
      found = !found;

    else if (session.groups) {
      register int i = 0;

      for (i = session.groups->nelts-1; i >= 0; i--)
        if (strcmp(*(((char **) session.groups->elts) + i), grp) == 0) {
          found = !found;
          break;
        }
    }

    if (found)
      return TRUE;
  }

  return FALSE;
}

/* Boolean "user-expression" AND matching, returns TRUE if the expression
 * evaluates to TRUE.
 */
int pr_expr_eval_user_and(char **expr) {
  int found;
  char *user;

  for (; *expr; expr++) {
    user = *expr;
    found = FALSE;

    if (*user == '!') {
      found = !found;
      user++;
    }

    if (strcmp(session.user, user) == 0)
      found = !found;

    if (!found) 
      return FALSE;
  }

  return TRUE;
}

/* Boolean "user-expression" OR matching, returns TRUE if the expression
 * evaluates to TRUE.
 */
int pr_expr_eval_user_or(char **expr) {
  int found;
  char *user;

  for (; *expr; expr++) {
    user = *expr;
    found = FALSE;

    if (*user == '!') {
      found = !found;
      user++;
    }

    if (strcmp(session.user, user) == 0)
      found = !found;

    if (found)
      return TRUE;
  }

  return FALSE;
}
