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
	char *			shadow;
	size_t			size;
	struct slbt_exec_ctx	ctx;
	char *			vbuffer[];
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


static char * slbt_source_file(char ** argv)
{
	char **	parg;
	char *	ch;

	for (parg=argv; *parg; parg++)
		if ((ch = strrchr(*parg,'.')))
			if ((!(strcmp(++ch,"s")))
					|| (!(strcmp(ch,"S")))
					|| (!(strcmp(ch,"asm")))
					|| (!(strcmp(ch,"c")))
					|| (!(strcmp(ch,"cc")))
					|| (!(strcmp(ch,"cxx"))))
				return *parg;
	return 0;
}


static struct slbt_exec_ctx_impl * slbt_exec_ctx_alloc(
	const struct slbt_driver_ctx *	dctx)
{
	struct slbt_exec_ctx_impl *	ictx;
	size_t				size;
	size_t				vsize;
	int				argc;
	char *				args;
	char *				shadow;
	char *				csrc;
	char **				parg;

	argc = 0;
	csrc = 0;

	/* clerical [worst-case] buffer size (guard, .libs, version) */
	size  = strlen(".lo") + sizeof('\0');
	size +=  8 * (strlen(".libs/") + sizeof('\0'));
	size += 36 * (strlen(".0000") + sizeof('\0'));

	/* buffer size (cargv, -Wc) */
	for (parg=dctx->cctx->cargv; *parg; parg++, argc++)
		if (!(strncmp("-Wc,",*parg,4)))
			size += sizeof('\0') + slbt_parse_comma_separated_flags(
					&(*parg)[4],&argc);
		else
			size += sizeof('\0') + strlen(*parg);

	/* buffer size (ldirname, lbasename, lobjname, aobjname, ltobjname) */
	if (dctx->cctx->output)
		size += 4*strlen(dctx->cctx->output);
	else if ((csrc = slbt_source_file(dctx->cctx->cargv)))
		size += 4*strlen(csrc);

	/* pessimistic argc: .libs/libfoo.so --> -L.libs -lfoo */
	argc *= 2;
	argc += SLBT_ARGV_SPARE_PTRS;

	/* buffer size (.libs/%.o, pessimistic) */
	size += argc * strlen(".libs/-L-l");

	/* buffer size (linking) */
	if (dctx->cctx->mode == SLBT_MODE_LINK)
		size += strlen(dctx->cctx->settings.arprefix) + sizeof('\0')
			+ strlen(dctx->cctx->settings.arsuffix) + sizeof('\0')
			+ strlen(dctx->cctx->settings.dsoprefix) + sizeof('\0')
			+ strlen(dctx->cctx->settings.dsoprefix) + sizeof('\0')
			+ strlen(dctx->cctx->settings.exeprefix) + sizeof('\0')
			+ strlen(dctx->cctx->settings.exeprefix) + sizeof('\0')
			+ strlen(dctx->cctx->settings.impprefix) + sizeof('\0')
			+ strlen(dctx->cctx->settings.impprefix) + sizeof('\0');

	/* alloc */
	if (!(args = malloc(size)))
		return 0;

	if (!(shadow = malloc(size))) {
		free(args);
		return 0;
	}

	/* altv: duplicate set, -Wl,--whole-archive, -Wl,--no-whole-archive */
	vsize = sizeof(*ictx) + 4*(argc+1)*sizeof(char *);

	if (!(ictx = calloc(1,vsize))) {
		free(args);
		free(shadow);
		return 0;
	}

	ictx->args = args;
	ictx->argc = argc;

	ictx->size   = size;
	ictx->shadow = shadow;

	ictx->ctx.csrc = csrc;

	return ictx;
}


int  slbt_get_exec_ctx(
	const struct slbt_driver_ctx *	dctx,
	struct slbt_exec_ctx **		ectx)
{
	struct slbt_exec_ctx_impl *	ictx;
	char **				parg;
	char *				ch;
	char *				mark;
	char *				slash;
	const char *			ref;
	int				i;

	/* alloc */
	if (!(ictx = slbt_exec_ctx_alloc(dctx)))
		return -1;

	/* init with guard for later .lo check */
	ch                = ictx->args + strlen(".lo");
	ictx->ctx.argv    = ictx->vbuffer;
	ictx->ctx.altv    = &ictx->vbuffer[ictx->argc + 1];

	/* <compiler> */
	ictx->ctx.compiler = dctx->cctx->cargv[0];
	ictx->ctx.cargv    = ictx->ctx.argv;

	/* ldirname, lbasename */
	ref = (dctx->cctx->output)
		? dctx->cctx->output
		: ictx->ctx.csrc;

	if (ref && !ictx->ctx.csrc && (mark = strrchr(ref,'/'))) {
		ictx->ctx.ldirname = ch;
		strcpy(ch,ref);
		ch += mark - ref;
		ch += sprintf(ch,"%s","/.libs/");
		ch++;

		ictx->ctx.lbasename = ch;
		ch += sprintf(ch,"%s",++mark);
		ch++;
	} else if (ref) {
		ictx->ctx.ldirname = ch;
		ch += sprintf(ch,"%s",".libs/");
		ch++;

		ictx->ctx.lbasename = ch;
		mark = strrchr(ref,'/');
		ch += sprintf(ch,"%s",mark ? ++mark : ref);
		ch++;
	}

	/* lbasename suffix */
	if (ref && (dctx->cctx->mode == SLBT_MODE_COMPILE)) {
		if ((ch[-4] == '.') && (ch[-3] == 'l') && (ch[-2] == 'o')) {
			ch[-3] = 'o';
			ch[-2] = '\0';
			ch--;
		} else if (ictx->ctx.csrc) {
			if ((mark = strrchr(ictx->ctx.lbasename,'.'))) {
				ch    = mark;
				*++ch = 'o';
				*++ch = '\0';
				ch++;
			}
		}
	}

	/* cargv, -Wc */
	for (i=0, parg=dctx->cctx->cargv; *parg; parg++, ch++) {
		if (!(strncmp("-Wc,",*parg,4))) {
			strcpy(ch,&(*parg)[4]);
			ictx->ctx.argv[i++] = ch;

			for (; *ch; ch++)
				if (*ch == ',') {
					*ch++ = '\0';
					ictx->ctx.argv[i++] = ch;
				}
		} else {
			ictx->ctx.argv[i++] = ch;
			ch += sprintf(ch,"%s",*parg);
			ch += strlen(".libs/-L-l");
		}
	}

	/* placeholders for -DPIC, -fPIC, -c, -o, <output> */
	ictx->ctx.dpic = &ictx->ctx.argv[i++];
	ictx->ctx.fpic = &ictx->ctx.argv[i++];
	ictx->ctx.cass = &ictx->ctx.argv[i++];

	ictx->ctx.noundef = &ictx->ctx.argv[i++];
	ictx->ctx.soname  = &ictx->ctx.argv[i++];
	ictx->ctx.lsoname = &ictx->ctx.argv[i++];

	ictx->ctx.lout[0] = &ictx->ctx.argv[i++];
	ictx->ctx.lout[1] = &ictx->ctx.argv[i++];
	ictx->ctx.sentinel= &ictx->ctx.argv[i++];

	slbt_reset_placeholders(&ictx->ctx);

	/* output file name */
	if (ref && ((dctx->cctx->mode == SLBT_MODE_COMPILE))) {
		*ictx->ctx.lout[0] = "-o";
		*ictx->ctx.lout[1] = ch;
		ictx->ctx.lobjname = ch;

		ch += sprintf(ch,"%s%s",
			ictx->ctx.ldirname,
			ictx->ctx.lbasename)
			+ sizeof('\0');

		ictx->ctx.aobjname = ch;

		ch += sprintf(ch,"%s",ictx->ctx.ldirname);
		ch -= strlen(".libs/");
		ch += sprintf(ch,"%s",
			ictx->ctx.lbasename)
			+ sizeof('\0');

		ictx->ctx.ltobjname = ch;
		strcpy(ch,ictx->ctx.aobjname);

		if ((mark = strrchr(ch,'.')))
			ch = mark + sprintf(mark,"%s",".lo")
				+ sizeof('\0');
	}

	/* linking: arfilename, lafilename, dsofilename */
	if (dctx->cctx->mode == SLBT_MODE_LINK && dctx->cctx->libname) {
		/* arfilename */
		ictx->ctx.arfilename = ch;
		ch += sprintf(ch,"%s%s%s%s",
				ictx->ctx.ldirname,
				dctx->cctx->settings.arprefix,
				dctx->cctx->libname,
				dctx->cctx->settings.arsuffix)
			+ sizeof('\0');



		/* lafilename */
		ictx->ctx.lafilename = ch;
		ch += sprintf(ch,"%s%s%s.la",
				ictx->ctx.ldirname,
				(dctx->cctx->drvflags & SLBT_DRIVER_MODULE)
					? ""
					: dctx->cctx->settings.dsoprefix,
				dctx->cctx->libname)
			+ sizeof('\0');


		/* dsofilename */
		ictx->ctx.dsofilename = ch;
		ch += sprintf(ch,"%s%s%s%s",
				ictx->ctx.ldirname,
				(dctx->cctx->drvflags & SLBT_DRIVER_MODULE)
					? ""
					: dctx->cctx->settings.dsoprefix,
				dctx->cctx->libname,
				dctx->cctx->settings.dsosuffix)
			+ sizeof('\0');
	}

	/* linking: exefilename */
	if (dctx->cctx->mode == SLBT_MODE_LINK && !dctx->cctx->libname) {
		ictx->ctx.exefilename = ch;

		if ((slash = strrchr(dctx->cctx->output,'/'))) {
			strcpy(ch,dctx->cctx->output);
			mark = ch + (slash - dctx->cctx->output);
			sprintf(++mark,".libs/%s",++slash);
			ch += strlen(ch) + sizeof('\0');
		} else
			ch += sprintf(ch,".libs/%s",dctx->cctx->output)
				+ sizeof('\0');
	}

	/* argument strings shadow copy */
	memcpy(ictx->shadow,ictx->args,ictx->size);

	*ectx = &ictx->ctx;
	return 0;
}


static int slbt_free_exec_ctx_impl(
	struct slbt_exec_ctx_impl *	ictx,
	int				status)
{
	if (ictx->ctx.fwrapper)
		fclose(ictx->ctx.fwrapper);

	free(ictx->args);
	free(ictx->shadow);
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


void slbt_reset_arguments(struct slbt_exec_ctx * ectx)
{
	struct slbt_exec_ctx_impl *	ictx;
	uintptr_t			addr;

	addr = (uintptr_t)ectx - offsetof(struct slbt_exec_ctx_impl,ctx);
	ictx = (struct slbt_exec_ctx_impl *)addr;
	memcpy(ictx->args,ictx->shadow,ictx->size);
}


void slbt_reset_placeholders(struct slbt_exec_ctx * ectx)
{
	*ectx->dpic = "-USLIBTOOL_PLACEHOLDER_DPIC";
	*ectx->fpic = "-USLIBTOOL_PLACEHOLDER_FPIC";
	*ectx->cass = "-USLIBTOOL_PLACEHOLDER_COMPILE_ASSEMBLE";

	*ectx->noundef = "-USLIBTOOL_PLACEHOLDER_NO_UNDEFINED";
	*ectx->soname  = "-USLIBTOOL_PLACEHOLDER_SONAME";
	*ectx->lsoname = "-USLIBTOOL_PLACEHOLDER_LSONAME";

	*ectx->lout[0] = "-USLIBTOOL_PLACEHOLDER_OUTPUT_SWITCH";
	*ectx->lout[1] = "-USLIBTOOL_PLACEHOLDER_OUTPUT_FILE";
	*ectx->sentinel= 0;
}

void slbt_disable_placeholders(struct slbt_exec_ctx * ectx)
{
	*ectx->dpic = 0;
	*ectx->fpic = 0;
	*ectx->cass = 0;

	*ectx->noundef = 0;
	*ectx->soname  = 0;
	*ectx->lsoname = 0;

	*ectx->lout[0] = 0;
	*ectx->lout[1] = 0;
	*ectx->sentinel= 0;
}
