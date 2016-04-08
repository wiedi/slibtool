/*******************************************************************/
/*  slibtool: a skinny libtool implementation, written in C        */
/*  Copyright (C) 2016  Z. Gilboa                                  */
/*  Released under the Standard MIT License; see COPYING.SLIBTOOL. */
/*******************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "slibtool_symlink_impl.h"

int slbt_create_symlink(
	const struct slbt_driver_ctx *	dctx,
	struct slbt_exec_ctx *		ectx,
	const char *			target,
	char *				lnkname,
	bool				flawrapper)
{
	const char *	slash;
	char *		ln[5];
	char *		dotdot;
	char		tmplnk [PATH_MAX];
	char		atarget[PATH_MAX];

	/* atarget */
	if ((slash = strrchr(target,'/')))
		slash++;
	else
		slash = target;

	dotdot = flawrapper ? "../" : "";

	if ((size_t)snprintf(atarget,sizeof(atarget),"%s%s",
			dotdot,slash) >= sizeof(atarget))
		return -1;

	/* tmplnk */
	if ((size_t)snprintf(tmplnk,sizeof(tmplnk),"%s.symlink.tmp",
			lnkname) >= sizeof(tmplnk))
		return -1;

	/* ln argv (fake) */
	ln[0] = "ln";
	ln[1] = "-s";
	ln[2] = atarget;
	ln[3] = lnkname;
	ln[4] = 0;
	ectx->argv = ln;

	/* step output */
	if (!(dctx->cctx->drvflags & SLBT_DRIVER_SILENT))
		if (slbt_output_link(dctx,ectx))
			return -1;

	/* create symlink */
	if (symlink(atarget,tmplnk))
		return -1;

	return rename(tmplnk,lnkname);
}
