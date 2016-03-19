/*******************************************************************/
/*  slibtool: a skinny libtool implementation, written in C        */
/*  Copyright (C) 2016  Z. Gilboa                                  */
/*  Released under the Standard MIT License; see COPYING.SLIBTOOL. */
/*******************************************************************/

#include <stdio.h>
#include <slibtool/slibtool.h>

int slbt_output_compile(
	const struct slbt_driver_ctx *	dctx,
	const struct slbt_exec_ctx *	ectx)
{
	char ** parg;

	if (fprintf(stdout,"%s: compile:",dctx->program) < 0)
		return -1;

	for (parg=ectx->argv; *parg; parg++)
		if (fprintf(stdout," %s",*parg) < 0)
			return -1;

	if (fputc('\n',stdout) < 0)
		return -1;

	return 0;
}
