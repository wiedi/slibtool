#ifndef SLIBTOOL_UNINSTALL_IMPL_H
#define SLIBTOOL_UNINSTALL_IMPL_H

#include "argv/argv.h"

extern const struct argv_option slbt_uninstall_options[];

enum uninstall_tags {
	TAG_UNINSTALL_HELP,
	TAG_UNINSTALL_VERSION,
	TAG_UNINSTALL_FORCE,
	TAG_UNINSTALL_RMDIR,
	TAG_UNINSTALL_VERBOSE,
	TAG_UNINSTALL_FORBIDDEN,
	TAG_UNINSTALL_RECURSIVE = TAG_UNINSTALL_FORBIDDEN,
};

#define SLBT_UNINSTALL_HELP		0x0001
#define SLBT_UNINSTALL_VERSION		0x0002
#define SLBT_UNINSTALL_FORCE		0x0004
#define SLBT_UNINSTALL_RMDIR		0x0008
#define SLBT_UNINSTALL_VERBOSE		0x0010
#define SLBT_UNINSTALL_FORBIDDEN	0x8000

#endif