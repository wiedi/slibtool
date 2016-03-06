/*******************************************************************/
/*  slibtool: a skinny libtool implementation, written in C        */
/*  Copyright (C) 2016  Z. Gilboa                                  */
/*  Released under the Standard MIT License; see COPYING.SLIBTOOL. */
/*******************************************************************/

#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>

#define ARGV_DRIVER

#include <slibtool/slibtool.h>
#include "slibtool_driver_impl.h"
#include "argv/argv.h"

struct slbt_driver_ctx_alloc {
	struct argv_meta *		meta;
	struct slbt_driver_ctx_impl	ctx;
	uint64_t			guard;
	const char *			units[];
};

static uint32_t slbt_argv_flags(uint32_t flags)
{
	uint32_t ret = 0;

	if (flags & SLBT_DRIVER_VERBOSITY_NONE)
		ret |= ARGV_VERBOSITY_NONE;

	if (flags & SLBT_DRIVER_VERBOSITY_ERRORS)
		ret |= ARGV_VERBOSITY_ERRORS;

	if (flags & SLBT_DRIVER_VERBOSITY_STATUS)
		ret |= ARGV_VERBOSITY_STATUS;

	return ret;
}

static int slbt_driver_usage(
	const char *			program,
	const char *			arg,
	const struct argv_option *	options,
	struct argv_meta *		meta)
{
	char header[512];

	snprintf(header,sizeof(header),
		"Usage: %s [options] <file>...\n" "Options:\n",
		program);

	argv_usage(stdout,header,options,arg);
	argv_free(meta);

	return SLBT_USAGE;
}

static struct slbt_driver_ctx_impl * slbt_driver_ctx_alloc(
	struct argv_meta *		meta,
	const struct slbt_common_ctx *	cctx,
	size_t				nunits)
{
	struct slbt_driver_ctx_alloc *	ictx;
	size_t				size;
	struct argv_entry *		entry;
	const char **			units;

	size =  sizeof(struct slbt_driver_ctx_alloc);
	size += (nunits+1)*sizeof(const char *);

	if (!(ictx = calloc(1,size)))
		return 0;

	if (cctx)
		memcpy(&ictx->ctx.cctx,cctx,sizeof(*cctx));

	for (entry=meta->entries,units=ictx->units; entry->fopt || entry->arg; entry++)
		if (!entry->fopt)
			*units++ = entry->arg;

	ictx->meta = meta;
	ictx->ctx.ctx.units = ictx->units;
	return &ictx->ctx;
}

static int slbt_get_driver_ctx_fail(struct argv_meta * meta)
{
	argv_free(meta);
	return -1;
}

int slbt_get_driver_ctx(
	char **				argv,
	char **				envp,
	uint32_t			flags,
	struct slbt_driver_ctx **	pctx)
{
	struct slbt_driver_ctx_impl *	ctx;
	struct slbt_common_ctx		cctx;
	const struct argv_option *	options;
	struct argv_meta *		meta;
	struct argv_entry *		entry;
	size_t				nunits;
	const char *			program;

	options = slbt_default_options;

	if (!(meta = argv_get(argv,options,slbt_argv_flags(flags))))
		return -1;

	nunits	= 0;
	program = argv_program_name(argv[0]);
	memset(&cctx,0,sizeof(cctx));

	if (!argv[1] && (flags & SLBT_DRIVER_VERBOSITY_USAGE))
		return slbt_driver_usage(program,0,options,meta);

	/* get options, count units */
	for (entry=meta->entries; entry->fopt || entry->arg; entry++) {
		if (entry->fopt) {
			switch (entry->tag) {
				case TAG_HELP:
				case TAG_HELP_ALL:
					if (flags & SLBT_DRIVER_VERBOSITY_USAGE)
						return slbt_driver_usage(program,entry->arg,options,meta);

				case TAG_VERSION:
					cctx.drvflags |= SLBT_DRIVER_VERSION;
					break;

				case TAG_MODE:
					if (!strcmp("clean",entry->arg))
						cctx.mode = SLBT_MODE_CLEAN;

					else if (!strcmp("compile",entry->arg))
						cctx.mode = SLBT_MODE_COMPILE;

					else if (!strcmp("execute",entry->arg))
						cctx.mode = SLBT_MODE_EXECUTE;

					else if (!strcmp("finish",entry->arg))
						cctx.mode = SLBT_MODE_FINISH;

					else if (!strcmp("install",entry->arg))
						cctx.mode = SLBT_MODE_INSTALL;

					else if (!strcmp("link",entry->arg))
						cctx.mode = SLBT_MODE_LINK;

					else if (!strcmp("uninstall",entry->arg))
						cctx.mode = SLBT_MODE_UNINSTALL;
					break;

				case TAG_DRY_RUN:
					cctx.drvflags |= SLBT_DRIVER_DRY_RUN;
					break;

				case TAG_TAG:
					if (!strcmp("CC",entry->arg))
						cctx.tag = SLBT_TAG_CC;

					else if (!strcmp("CXX",entry->arg))
						cctx.tag = SLBT_TAG_CXX;
					break;

				case TAG_CONFIG:
					cctx.drvflags |= SLBT_DRIVER_CONFIG;
					break;

				case TAG_DEBUG:
					cctx.drvflags |= SLBT_DRIVER_DEBUG;
					break;

				case TAG_FEATURES:
					cctx.drvflags |= SLBT_DRIVER_FEATURES;
					break;
			}
		} else
			nunits++;
	}

	if (!(ctx = slbt_driver_ctx_alloc(meta,&cctx,nunits)))
		return slbt_get_driver_ctx_fail(meta);

	ctx->ctx.program	= program;
	ctx->ctx.cctx		= &ctx->cctx;

	*pctx = &ctx->ctx;
	return SLBT_OK;
}

int slbt_create_driver_ctx(
	const struct slbt_common_ctx *	cctx,
	struct slbt_driver_ctx **	pctx)
{
	struct argv_meta *		meta;
	struct slbt_driver_ctx_impl *	ctx;
	char *				argv[] = {"slibtool_driver",0};

	if (!(meta = argv_get(argv,slbt_default_options,0)))
		return -1;

	if (!(ctx = slbt_driver_ctx_alloc(meta,cctx,0)))
		return slbt_get_driver_ctx_fail(0);

	ctx->ctx.cctx = &ctx->cctx;
	memcpy(&ctx->cctx,cctx,sizeof(*cctx));
	*pctx = &ctx->ctx;
	return SLBT_OK;
}

static void slbt_free_driver_ctx_impl(struct slbt_driver_ctx_alloc * ictx)
{
	argv_free(ictx->meta);
	free(ictx);
}

void slbt_free_driver_ctx(struct slbt_driver_ctx * ctx)
{
	struct slbt_driver_ctx_alloc *	ictx;
	uintptr_t			addr;

	if (ctx) {
		addr = (uintptr_t)ctx - offsetof(struct slbt_driver_ctx_alloc,ctx);
		addr = addr - offsetof(struct slbt_driver_ctx_impl,ctx);
		ictx = (struct slbt_driver_ctx_alloc *)addr;
		slbt_free_driver_ctx_impl(ictx);
	}
}
