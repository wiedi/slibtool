/*******************************************************************/
/*  slibtool: a skinny libtool implementation, written in C        */
/*  Copyright (C) 2016  Z. Gilboa                                  */
/*  Released under the Standard MIT License; see COPYING.SLIBTOOL. */
/*******************************************************************/

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <slibtool/slibtool.h>

#define SLBT_ARGV_SPARE_PTRS	16

struct slbt_exec_ctx_impl {
	int			argc;
	char *			args;
	struct slbt_exec_ctx	ctx;
	char *			buffer[];
};


static size_t slbt_parse_comma_separated_flags(
	const char *	str,
	int *		argc)
{
	const char * ch;

	for (ch=str; *ch; ch++)
		if (*ch == ',')
			(*argc)++;

	return ch - str;
}


static struct slbt_exec_ctx_impl * slbt_exec_ctx_alloc(
	const struct slbt_driver_ctx *	dctx)
{
	struct slbt_exec_ctx_impl *	ictx;
	size_t				size;
	int				argc;
	char *				args;
	char **				parg;

	size = argc = 0;

	/* buffer size (cargv, -Wc) */
	for (parg=dctx->cctx->cargv; *parg; parg++, argc++)
		if (!(strncmp("-Wc,",*parg,4)))
			size += sizeof('\0') + slbt_parse_comma_separated_flags(
					parg[4],&argc);
		else
			size += sizeof('\0') + strlen(*parg);

	/* alloc */
	if (!(args = malloc(size + strlen(".libs/")
				 + strlen(".lo")
				 + strlen(dctx->cctx->output))))
		return 0;

	size = sizeof(*ictx) + (argc+SLBT_ARGV_SPARE_PTRS)*sizeof(char *);

	if (!(ictx = calloc(1,size))) {
		free(args);
		return 0;
	}

	ictx->args = args;
	ictx->argc = argc;

	return ictx;
}


int  slbt_get_exec_ctx(
	const struct slbt_driver_ctx *	dctx,
	struct slbt_exec_ctx **		ectx)
{
	struct slbt_exec_ctx_impl *	ictx;
	char **				parg;
	char *				ch;
	char *				slash;
	int				i;

	/* alloc */
	if (!(ictx = slbt_exec_ctx_alloc(dctx)))
		return -1;

	/* init with guard for later .lo check */
	ch                = ictx->args + strlen(".lo");
	ictx->ctx.argv    = ictx->buffer;

	/* <compiler> */
	ictx->ctx.program = dctx->cctx->cargv[0];

	/* cargv, -Wc */
	for (i=0, parg=dctx->cctx->cargv; *parg; parg++, ch++) {
		if (!(strncmp("-Wc,",*parg,4))) {
			strcpy(ch,parg[4]);
			ictx->ctx.argv[i++] = ch;

			for (; *ch; ch++)
				if (*ch == ',') {
					*ch = '\0';
					ictx->ctx.argv[i++] = ch+1;
				}
		} else {
			ictx->ctx.argv[i++] = ch;
			ch += sprintf(ch,"%s",*parg);
		}
	}

	if (dctx->cctx->mode == SLBT_MODE_COMPILE) {
		if (dctx->cctx->drvflags & SLBT_DRIVER_SHARED) {
			ictx->ctx.argv[i++] = "-DPIC";
			ictx->ctx.argv[i++] = "-fPIC";
		}

		ictx->ctx.argv[i++] = "-c";
		ictx->ctx.argv[i++] = "-o";
		ictx->ctx.argv[i++] = ch;

		if ((slash = strrchr(dctx->cctx->output,'/'))) {
			sprintf(ch,"%s",dctx->cctx->output);
			ch += slash - dctx->cctx->output;
			ch += sprintf(ch,"/.libs%s",slash);
		} else
			ch += sprintf(ch,".libs/%s",dctx->cctx->output);

		if ((ch[-3] == '.') && (ch[-2] == 'l') && (ch[-1] == 'o')) {
			ch[-2] = 'o';
			ch[-1] = '\0';
			ch--;
		}
	}

	*ectx = &ictx->ctx;
	return 0;
}


static int slbt_free_exec_ctx_impl(
	struct slbt_exec_ctx_impl *	ictx,
	int				status)
{
	free(ictx->args);
	free (ictx);
	return status;
}


void slbt_free_exec_ctx(struct slbt_exec_ctx * ctx)
{
	struct slbt_exec_ctx_impl *	ictx;
	uintptr_t			addr;

	if (ctx) {
		addr = (uintptr_t)ctx - offsetof(struct slbt_exec_ctx_impl,ctx);
		ictx = (struct slbt_exec_ctx_impl *)addr;
		slbt_free_exec_ctx_impl(ictx,0);
	}
}
