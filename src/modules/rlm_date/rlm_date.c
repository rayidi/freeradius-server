/*
 *   This program is is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or (at
 *   your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/**
 * @file rlm_date.c
 * @brief Translates timestrings between formats.
 *
 * @author Artur Malinowski <artur@wow.com>
 *
 * @copyright 2013 Artur Malinowski <artur@wow.com>
 * @copyright 1999-2013 The FreeRADIUS Server Project.
 */

#include <freeradius-devel/radiusd.h>
#include <freeradius-devel/modules.h>
#include <ctype.h>
#include <time.h>

typedef struct rlm_date_t {
	char const *xlat_name;
	char const *fmt;
} rlm_date_t;

static const CONF_PARSER module_config[] = {
	{ FR_CONF_OFFSET("format", FR_TYPE_STRING, rlm_date_t, fmt), .dflt = "%b %e %Y %H:%M:%S %Z" },
	CONF_PARSER_TERMINATOR
};

DIAG_OFF(format-nonliteral)
static ssize_t xlat_date_convert(UNUSED TALLOC_CTX *ctx, char **out, size_t outlen,
				 void const *mod_inst, UNUSED void const *xlat_inst,
				 REQUEST *request, char const *fmt)
{
	rlm_date_t const *inst = mod_inst;
	time_t date = 0;
	struct tm tminfo;
	VALUE_PAIR *vp;

	memset(&tminfo, 0, sizeof(tminfo));

	if ((radius_get_vp(&vp, request, fmt) < 0) || !vp) return 0;

	switch (vp->vp_type) {
	/*
	 *	These are 'to' types, i.e. we'll convert the integers
	 *	to a time structure, and then output it in the specified
	 *	format as a string.
	 */
	case FR_TYPE_DATE:
		date = vp->vp_date;
		goto encode;

	case FR_TYPE_UINT32:
	case FR_TYPE_UINT64:
		date = (time_t) vp->vp_uint32;

	encode:
		if (localtime_r(&date, &tminfo) == NULL) {
			REDEBUG("Failed converting time string to localtime");
			goto error;
		}
		return strftime(*out, outlen, inst->fmt, &tminfo);

	/*
	 *	These are 'from' types, i.e. we'll convert the input string
	 *	into a time structure, and then output it as an integer
	 *	unix timestamp.
	 */
	case FR_TYPE_STRING:
		if (strptime(vp->vp_strvalue, inst->fmt, &tminfo) == NULL) {
			REDEBUG("Failed to parse time string \"%s\" as format '%s'", vp->vp_strvalue, inst->fmt);
			goto error;
		}

		date = mktime(&tminfo);
		if (date < 0) {
			REDEBUG("Failed converting parsed time into unix time");

		}
		return snprintf(*out, outlen, "%" PRIu64, (uint64_t) date);

	default:
		REDEBUG("Can't convert type %s into date", fr_int2str(dict_attr_types, vp->da->type, "<INVALID>"));
	}

error:
	return -1;
}
DIAG_ON(format-nonliteral)

static int mod_bootstrap(CONF_SECTION *conf, void *instance)
{
	rlm_date_t *inst = instance;

	inst->xlat_name = cf_section_name2(conf);
	if (!inst->xlat_name) {
		inst->xlat_name = cf_section_name1(conf);
	}

	xlat_register(inst, inst->xlat_name, xlat_date_convert, NULL, NULL, 0, XLAT_DEFAULT_BUF_LEN);

	return 0;
}

extern rad_module_t rlm_date;
rad_module_t rlm_date = {
	.magic		= RLM_MODULE_INIT,
	.name		= "date",
	.inst_size	= sizeof(rlm_date_t),
	.config		= module_config,
	.bootstrap	= mod_bootstrap
};

