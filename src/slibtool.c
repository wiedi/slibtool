/*******************************************************************/
/*  slibtool: a skinny libtool implementation, written in C        */
/*  Copyright (C) 2016  Z. Gilboa                                  */
/*  Released under the Standard MIT License; see COPYING.SLIBTOOL. */
/*******************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <slibtool/slibtool.h>
#include "slibtool_version.h"
#include "slibtool_driver_impl.h"

#ifndef SLBT_DRIVER_FLAGS
#define SLBT_DRIVER_FLAGS	SLBT_DRIVER_VERBOSITY_ERRORS \
				| SLBT_DRIVER_VERBOSITY_USAGE
#endif

static const char vermsg[] = "%s (git://midipix.org/slibtool): commit %s.\n";

static ssize_t slibtool_version(struct slbt_driver_ctx * dctx)
{
	return fprintf(stdout,vermsg,dctx->program,SLIBTOOL_GIT_VERSION);
}

static void slibtool_perform_driver_actions(struct slbt_driver_ctx * dctx)
{
	if (dctx->cctx->drvflags & SLBT_DRIVER_CONFIG)
		dctx->nerrors += (slbt_output_config(dctx) < 0);

	if (dctx->cctx->mode == SLBT_MODE_COMPILE)
		dctx->nerrors += (slbt_exec_compile(dctx,0) < 0);

	if (dctx->cctx->mode == SLBT_MODE_LINK)
		dctx->nerrors += (slbt_exec_link(dctx,0) < 0);
}

static void slibtool_perform_unit_actions(struct slbt_unit_ctx * uctx)
{
}

static int slibtool_exit(struct slbt_driver_ctx * dctx, int nerrors)
{
	slbt_free_driver_ctx(dctx);
	return nerrors ? 2 : 0;
}

int slibtool_main(int argc, char ** argv, char ** envp)
{
	int				ret;
	struct slbt_driver_ctx *	dctx;
	struct slbt_unit_ctx *		uctx;
	const char **			unit;

	if ((ret = slbt_get_driver_ctx(argv,envp,SLBT_DRIVER_FLAGS,&dctx)))
		return (ret == SLBT_USAGE) ? !--argc : 2;

	if (dctx->cctx->drvflags & SLBT_DRIVER_VERSION)
		if ((slibtool_version(dctx)) < 0)
			return slibtool_exit(dctx,2);

	slibtool_perform_driver_actions(dctx);
	ret += dctx->nerrors;

	for (unit=dctx->units; *unit; unit++) {
		if (!(slbt_get_unit_ctx(dctx,*unit,&uctx))) {
			slibtool_perform_unit_actions(uctx);
			ret += uctx->nerrors;
			slbt_free_unit_ctx(uctx);
		}
	}

	return slibtool_exit(dctx,ret);
}

#ifndef SLIBTOOL_IN_A_BOX

int main(int argc, char ** argv, char ** envp)
{
	return slibtool_main(argc,argv,envp);
}

#endif
