/*
 * ProFTPD - FTP server daemon
 * Copyright (c) 1997, 1998 Public Flood Software
 * Copyright (c) 2003 The ProFTPD Project team
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
 * As a special exemption, the copyright holders give permission to link
 * this program with OpenSSL and distribute the resulting executable without
 * including the source code for OpenSSL in the source distribution.
 */

/* Use POSIX "capabilities" in modern operating systems (currently, only
 * Linux is supported) to severely limit root's access. After user
 * authentication, this module _completely_ gives up root, except for the
 * bare minimum functionality that is required. VERY highly recommended for
 * security-consious admins. See README.capabilities for more information.
 *
 * -- DO NOT MODIFY THE TWO LINES BELOW --
 * $Libraries: -Llib/libcap -lcap$
 * $Directories: lib/libcap$
 * $Id: mod_cap.c,v 1.6 2003-01-09 04:27:52 jwm Exp $
 */

#include <stdio.h>
#include <stdlib.h>


#ifdef LINUX
# ifdef __powerpc__
#  define _LINUX_BYTEORDER_GENERIC_H
# endif

# ifdef HAVE_LINUX_CAPABILITY_H
#  include <linux/capability.h>
# endif /* HAVE_LINUX_CAPABILITY_H */
# include "../lib/libcap/include/sys/capability.h"

/* What are these for? */
# undef WNOHANG
# undef WUNTRACED
#endif /* LINUX */

#include "conf.h"
#include "privs.h"

#define MOD_CAP_VERSION	"mod_cap/1.0"

static cap_t capabilities = 0;
static unsigned char use_capabilities = TRUE;
static unsigned char use_cap_chown = TRUE;

/* log current capabilities */
static void lp_debug(void) {
  char *res;
  ssize_t len;
  cap_t caps;

  if (! (caps = cap_get_proc())) {
    log_pri(PR_LOG_ERR, MOD_CAP_VERSION ": cap_get_proc failed: %s",
            strerror(errno));
    return;
  }

  if (! (res = cap_to_text(caps,&len))) {
    log_pri(PR_LOG_ERR, MOD_CAP_VERSION ": cap_to_text failed: %s",
            strerror(errno));
    cap_free(&caps);
    return;
  }

  log_debug(DEBUG1, MOD_CAP_VERSION ": capabilities '%s'.", res);
  cap_free(&caps);
  free(res);
}

/* create a new capability structure */
static int lp_init_cap(void) {
  if (! (capabilities = cap_init())) {
    log_pri(PR_LOG_ERR, MOD_CAP_VERSION ": cap_init failed: %s.",
            strerror(errno));
    return -1;
  }

  return 0;
}

/* free the capability structure */
static void lp_free_cap(void) {
  cap_free(&capabilities);
}

/* add a capability to a given set */
static int lp_add_cap(cap_value_t cap, cap_flag_t set) {
  if (cap_set_flag(capabilities, set, 1, &cap, CAP_SET) == -1) {
    log_pri(PR_LOG_ERR, MOD_CAP_VERSION ": cap_set_flag failed: %s",
            strerror(errno));
    return -1;
  }

  return 0;
}

/* send the capabilities to the kernel */
static int lp_set_cap(void) {
  if (cap_set_proc(capabilities) == -1) {
    log_pri(PR_LOG_ERR, MOD_CAP_VERSION ": cap_set_proc failed: %s",
            strerror(errno));
    return -1;
  }

  return 0;
}

/* Configuration handlers
 */

MODRET set_caps(cmd_rec *cmd) {
  unsigned char use_chown = TRUE;
  config_rec *c = NULL;
  register unsigned int i = 0;

  if (cmd->argc - 1 < 1)
    CONF_ERROR(cmd, "need at least one parameter");

  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  /* At the moment, only the CAP_CHOWN capability is handled by this
   * directive.
   */
  for (i = 1; i < cmd->argc; i++) {
    char *cp = cmd->argv[i];
    cp++;

    if (*cmd->argv[i] != '+' && *cmd->argv[i] != '-')
      CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, ": bad option: '",
        cmd->argv[i], "'", NULL));

    if (strcasecmp(cp, "CAP_CHOWN") == 0) {
      if (*cmd->argv[i] == '-')
        use_chown = FALSE;

    } else
      CONF_ERROR(cmd, pstrcat(cmd->tmp_pool, "unknown capability: '",
                 cp, "'", NULL));
  }

  c = add_config_param(cmd->argv[0], 1, NULL);
  c->argv[0] = pcalloc(c->pool, sizeof(unsigned char));
  *((unsigned char *) c->argv[0]) = use_chown;

  return HANDLED(cmd);
}

MODRET set_capengine(cmd_rec *cmd) {
  int bool = -1;
  config_rec *c = NULL;

  CHECK_ARGS(cmd, 1);
  CHECK_CONF(cmd, CONF_ROOT|CONF_VIRTUAL|CONF_GLOBAL);

  if ((bool = get_boolean(cmd, 1)) == -1)
    CONF_ERROR(cmd, "expecting Boolean parameter");

  c = add_config_param(cmd->argv[0], 1, NULL);
  c->argv[0] = pcalloc(c->pool, sizeof(unsigned char));
  *((unsigned char *) c->argv[0]) = bool;

  return HANDLED(cmd);
}

/* Command handlers
 */

/* The POST_CMD handler for "PASS" is only called after PASS has
 * successfully completed, which means authentication is successful,
 * so we can "tweak" our root access down to almost nothing.
 */
MODRET cap_post_pass(cmd_rec *cmd) {
  int ret;

  if (!use_capabilities)
    return DECLINED(cmd);

  pr_signals_block();

  /* glibc2.1 is BROKEN, seteuid() no longer lets one set euid to uid,
   * so we can't use PRIVS_ROOT/PRIVS_RELINQUISH. setreuid() is the
   * workaround.
   */
  if (setreuid(session.uid, 0) == -1) {
    log_pri(PR_LOG_ERR, MOD_CAP_VERSION
      ": setreuid: %s", strerror(errno));
    pr_signals_unblock();
    return DECLINED(cmd);
  }

  /* The only capability we need is CAP_NET_BIND_SERVICE (bind
   * ports < 1024).  Everything else can be discarded.  We set this
   * in CAP_PERMITTED set only, as when we switch away from root
   * we lose CAP_EFFECTIVE anyhow, and must reset it.
   */

  ret = lp_init_cap();
  if (ret != -1)
    ret = lp_add_cap(CAP_NET_BIND_SERVICE, CAP_PERMITTED);

  /* Add the CAP_CHOWN capability, unless explicitly configured not to.
   */
  if (use_cap_chown && ret != -1)
    ret = lp_add_cap(CAP_CHOWN, CAP_PERMITTED);

  if (ret != -1)
    ret = lp_set_cap();

  if (setreuid(0, session.uid) == -1) {
    log_pri(PR_LOG_ERR, MOD_CAP_VERSION ": setreuid: %s", strerror(errno));
    pr_signals_unblock();
    end_login(1);
  }
  pr_signals_unblock();

  /* Now our only capabilities consist of CAP_NET_BIND_SERVICE
   * (and possibly CAP_CHOWN), however in order to actually be able to bind
   * to low-numbered ports, we need the capability to be in the effective set.
   */

  if (ret != -1)
    ret = lp_add_cap(CAP_NET_BIND_SERVICE, CAP_EFFECTIVE);

  /* Add the CAP_CHOWN capability, unless explicitly configured not to.
   */
  if (use_cap_chown && ret != -1)
    ret = lp_add_cap(CAP_CHOWN, CAP_EFFECTIVE);

  if (ret != -1)
    ret = lp_set_cap();

  if (capabilities)
    lp_free_cap();

  if (ret != -1) {
    /* That's it!  Disable all further id switching */
    session.disable_id_switching = TRUE;
    lp_debug();

  } else
    log_pri(PR_LOG_NOTICE, MOD_CAP_VERSION ": attempt to configure "
            "capabilities failed, reverting to normal operation");

  return DECLINED(cmd);
}

/* Initialization routines
 */

static int cap_sess_init(void) {
  /* Check to see if the lowering of capabilities has been disabled in the
   * configuration file.
   */
  if (use_capabilities) {
    unsigned char *cap_engine = get_param_ptr(main_server->conf,
                                              "CapabilitiesEngine", FALSE);

    if (cap_engine && *cap_engine == FALSE) {
      log_debug(DEBUG3, MOD_CAP_VERSION ": lowering of capabilities disabled");
      use_capabilities = FALSE;
    }
  }

  /* Check for which specific capabilities to include/exclude. */
  if (use_capabilities) {
    config_rec *c = NULL;

    if ((c = find_config(main_server->conf, CONF_PARAM,
                         "CapabilitiesSet", FALSE)) != NULL)
    {
      /* use_cap_chown is stored in c->argv[0] */
      if (*((unsigned char *) c->argv[0]) == FALSE) {
        log_debug(DEBUG3, MOD_CAP_VERSION ": removing CAP_CHOWN capability");
        use_cap_chown = FALSE;
      }
    }
  }

  return 0;
}

static int cap_module_init(void) {
  /* Attempt to determine if we are running on a kernel that supports
   * capabilities. This allows binary distributions to include the module
   * even if it may not work.
   */
  if (!cap_get_proc() && errno == ENOSYS) {
    log_debug(DEBUG2, MOD_CAP_VERSION
              ": kernel does not support capabilities, disabling module");
    use_capabilities = FALSE;
  }

  return 0;
}


/* Module API tables
 */

static conftable cap_conftab[] = {
  { "CapabilitiesEngine", set_capengine, NULL },
  { "CapabilitiesSet",    set_caps,      NULL },
  { NULL, NULL, NULL }
};

static cmdtable cap_cmdtab[] = {
  { POST_CMD, C_PASS, G_NONE, cap_post_pass, TRUE, FALSE },
  { 0, NULL }
};

module cap_module = {
  NULL, NULL,

  /* Module API version */
  0x20,

  /* Module name */
  "cap",

  /* Module configuration handler table */
  cap_conftab,

  /* Module command handler table */
  cap_cmdtab,

  /* Module authentication handler table */
  NULL,

  /* Module initialization */
  cap_module_init,

  /* Session initialization */
  cap_sess_init
};
