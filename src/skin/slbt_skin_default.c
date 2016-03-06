#include "slibtool_driver_impl.h"
#include "argv/argv.h"

const struct argv_option slbt_default_options[] = {
	{"version",		0,TAG_VERSION,ARGV_OPTARG_NONE,0,0,0,
				"show version information"},

	{"help",		'h',TAG_HELP,ARGV_OPTARG_OPTIONAL,0,0,0,
				"show usage information"},

	{0,0,0,0,0,0,0,0}
};
