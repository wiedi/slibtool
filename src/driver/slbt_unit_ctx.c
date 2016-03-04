/*******************************************************************/
/*  slibtool: a skinny libtool implementation, written in C        */
/*  Copyright (C) 2016  Z. Gilboa                                  */
/*  Released under the Standard MIT License; see COPYING.SLIBTOOL. */
/*******************************************************************/

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include <slibtool/slibtool.h>
#include "slibtool_driver_impl.h"

static int slbt_free_unit_ctx_impl(struct slbt_unit_ctx_impl * ctx, int status)
{
	if (ctx) {
		slbt_unmap_input(&ctx->map);
		free(ctx);
	}

	return status;
}

int slbt_get_unit_ctx(
	const struct slbt_driver_ctx *	dctx,
	const char *			path,
	struct slbt_unit_ctx **		pctx)
{
	struct slbt_unit_ctx_impl *	ctx;

	if (!dctx || !(ctx = calloc(1,sizeof(*ctx))))
		return -1;

	if (slbt_map_input(-1,path,PROT_READ,&ctx->map))
		return slbt_free_unit_ctx_impl(ctx,-1);

	memcpy(&ctx->cctx,dctx->cctx,
		sizeof(ctx->cctx));

	ctx->path	= path;

	ctx->uctx.path	= &ctx->path;
	ctx->uctx.map	= &ctx->map;
	ctx->uctx.cctx	= &ctx->cctx;

	*pctx = &ctx->uctx;
	return 0;
}

void slbt_free_unit_ctx(struct slbt_unit_ctx * ctx)
{
	struct slbt_unit_ctx_impl *	ictx;
	uintptr_t			addr;

	if (ctx) {
		addr = (uintptr_t)ctx - offsetof(struct slbt_unit_ctx_impl,uctx);
		ictx = (struct slbt_unit_ctx_impl *)addr;
		slbt_free_unit_ctx_impl(ictx,0);
	}
}
