/*
 * Copyright (C) 2012 Kian Mohageri (kian.mohageri@gmail.com).
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <net/pfvar.h>

#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 
 * If a threshold isn't specified, it will be based upon
 * the current hard state limits in pf(4)
 */
#define DEFAULT_WARN_PERCENT 80
#define DEFAULT_CRIT_PERCENT 90

/* return values used by Nagios */
enum {
    STATE_OK,
    STATE_WARNING,
    STATE_CRITICAL,
    STATE_UNKNOWN
};

static void version(void);
static void usage(void);
static void help(void);

const  char *revision = "0.1";
extern char *__progname;

/*
 * Check the status of PF (enabled/disabled) and whether
 * the number of states is high enough to meet the thresholds.
 * Print a single line for use by Nagios and return one of
 * the state values Nagios expects.
 */
int 
main(int argc, char *argv[])
{
	struct pf_status    ps;
	struct pfioc_limit  pl;
	const char          *errstr;
	const char          *pf_device;
	const char          *msg;
	float               percent;
	int                 ch, wflag, cflag, dev;
	int                 states_warning; 
	int                 states_critical;
	int                 ret;

	pf_device = "/dev/pf"; 

	wflag = cflag = 0;

	while ((ch = getopt(argc, argv, "Vhw:c:")) != -1) {
		switch (ch) {
		case 'V':
			version();
			exit(EXIT_FAILURE);
			break;
		case 'h':
			help();
			break;
		case 'w':
			wflag = 1;
			states_warning = strtonum(optarg, 0, UINT_MAX, &errstr);
			if (errstr) {
				printf("PF UNKNOWN - -w is %s: %s\n", errstr, optarg);
				return (STATE_UNKNOWN);
			}
			break;
		case 'c':
			cflag = 1;
			states_critical = strtonum(optarg, 0, UINT_MAX, &errstr);
			if (errstr) {
				printf("PF UNKNOWN - -c is %s: %s\n", errstr, optarg);
				return (STATE_UNKNOWN);
			}
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	
	dev = open(pf_device, O_RDONLY);
	if (dev == -1) {
		printf("PF UNKNOWN - open(\"%s\") failed\n", pf_device);
		return (STATE_UNKNOWN);
	}

	memset(&ps, 0, sizeof(struct pf_status));
	if (ioctl(dev, DIOCGETSTATUS, &ps) == -1) {
		printf("PF UNKNOWN - ioctl failed (DIOCGETSTATUS)\n");
		return (STATE_UNKNOWN);
	}

	memset(&pl, 0, sizeof(struct pfioc_limit));
	pl.index = PF_LIMIT_STATES;
	if (ioctl(dev, DIOCGETLIMIT, &pl) == -1) {
		printf("PF UNKNOWN - ioctl failed (DIOCGETLIMIT)\n");
		return (STATE_UNKNOWN);
	}

	/* default thresholds will be based on the current state limit */
	if (!wflag)
		states_warning = pl.limit * DEFAULT_WARN_PERCENT / 100;

	if (!cflag)
		states_critical = pl.limit * DEFAULT_CRIT_PERCENT / 100;

	if (states_warning >= states_critical) {
		printf("PF UNKNOWN - <warning> must be less than <critical>\n");
		return (STATE_UNKNOWN);
	}

	percent = (float)ps.states / (float)pl.limit * 100.0;

	if (ps.running != 1) {
		printf("PF CRITICAL - status: Disabled\n");
		return (STATE_CRITICAL);
	}

	if (ps.states >= states_critical) {
		msg = "CRITICAL";
		ret = STATE_CRITICAL;
	} else if (ps.states >= states_warning) {
		msg = "WARNING";
		ret = STATE_WARNING;
	} else {
		msg = "OK";
		ret = STATE_OK;
	}

	printf("PF %s - states: %u (%.1f%% - limit: %u) | states=%u;%u;%u;%u;%u\n",
	    msg, ps.states, percent, pl.limit,
	    ps.states, states_warning, states_critical, 0, pl.limit);

	return (ret);
}

static void
version(void)
{
	fprintf(stderr, "%s %s\n", __progname, revision);
}

static void
usage(void)
{
	fprintf(stderr,
	    "Usage: %s [-Vh] [-w number] [-c number]\n"
	    "\t-V         - Print the plugin version\n"
	    "\t-h         - Print the plugin help\n"
	    "\t-w number  - Warning when <number> states\n"
	    "\t-c number  - Critical when <number> states\n", 
	    __progname);
	exit(EXIT_FAILURE);
}

static void
help(void)
{
	version();
	fprintf(stderr, 
	    "\n"
	    "This plugin checks whether PF is enabled, and if so, the\n"
	    "number of states in the state table.\n"
	    "\n"
	    "The current state count is compared to the given or\n"
	    "default plugin thresholds and the appropriate Nagios state\n"
	    "is returned.\n"
	    "\n");
	usage();
}
