#ifndef SOFORT_DRIVER_IMPL_H
#define SOFORT_DRIVER_IMPL_H

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#include <slibtool/slibtool.h>
#include "argv/argv.h"

extern int slibtool_main(int, char **, char **);
extern const struct argv_option slbt_default_options[];

enum app_tags {
	TAG_HELP,
	TAG_HELP_ALL,
	TAG_VERSION,
	TAG_CONFIG,
	TAG_DEBUG,
	TAG_DRY_RUN,
	TAG_FEATURES,
	TAG_MODE,
	TAG_WARNINGS,
	TAG_DEPS,
	TAG_SILENT,
	TAG_TAG,
	TAG_VERBOSE,
};

struct slbt_driver_ctx_impl {
	struct slbt_common_ctx	cctx;
	struct slbt_driver_ctx	ctx;
};

struct slbt_unit_ctx_impl {
	const char *		path;
	struct slbt_input	map;
	struct slbt_common_ctx	cctx;
	struct slbt_unit_ctx	uctx;
};

#endif
