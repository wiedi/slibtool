/*******************************************************************/
/*  slibtool: a skinny libtool implementation, written in C        */
/*  Copyright (C) 2016  Z. Gilboa                                  */
/*  Released under the Standard MIT License; see COPYING.SLIBTOOL. */
/*******************************************************************/

#include <slibtool/slibtool.h>
#include "slibtool_driver_impl.h"
#include "slibtool_errinfo_impl.h"

int slbt_record_error(
	const struct slbt_driver_ctx *	dctx,
	int				syserror,
	int				liberror,
	const char *			function,
	int				line,
	unsigned			flags,
	void *				ctx)
{
	struct slbt_driver_ctx_impl *	ictx;
	struct slbt_error_info *	erri;

	ictx = slbt_get_driver_ictx(dctx);

	if (ictx->errinfp == ictx->erricap)
		return -1;

	*ictx->errinfp = &ictx->erribuf[ictx->errinfp - ictx->erriptr];
	erri = *ictx->errinfp;

	erri->syserror = syserror;
	erri->liberror = liberror;
	erri->function = function;
	erri->line     = line;
	erri->flags    = flags;
	erri->ctx      = ctx;

	ictx->errinfp++;

	return -1;
}
