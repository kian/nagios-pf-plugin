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
	float               percent;
	int                 ch, wflag, cflag, dev;
	int                 states_warning; 
	int                 states_critical;

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
			states_warning = strtonum(optarg, 0, ULONG_MAX, 
			    &errstr);
			if (errstr) {
				(void)printf("PF UNKNOWN - -w is %s: %s\n",
				    errstr, optarg);
				return (STATE_UNKNOWN);
			}
			break;
		case 'c':
			cflag = 1;
		        states_critical = strtonum(optarg, 0, ULONG_MAX,
			    &errstr);
			if (errstr) {
				(void)printf("PF UNKNOWN - -c is %s: %s\n",
				    errstr, optarg);
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
		(void)printf("PF UNKNOWN - open(\"%s\") failed\n", pf_device);
		return (STATE_UNKNOWN);
	}

	memset(&ps, 0, sizeof(struct pf_status));
	if (ioctl(dev, DIOCGETSTATUS, &ps) == -1) {
		(void)printf("PF UNKNOWN - ioctl failed (DIOCGETSTATUS)\n");
		return (STATE_UNKNOWN);
	}

	memset(&pl, 0, sizeof(struct pfioc_limit));
	pl.index = PF_LIMIT_STATES;
	if (ioctl(dev, DIOCGETLIMIT, &pl) == -1) {
		(void)printf("PF UNKNOWN - ioctl failed (DIOCGETLIMIT)\n");
		return (STATE_UNKNOWN);
	}

	/* default thresholds will be based on the current state limit */
	if (!wflag)
		states_warning = pl.limit * DEFAULT_WARN_PERCENT / 100;

	if (!cflag)
		states_critical = pl.limit * DEFAULT_CRIT_PERCENT / 100;

	if (states_warning >= states_critical) {
		(void)printf("PF UNKNOWN - <warning> must be less than "
            "<critical>\n");
		return (STATE_UNKNOWN);
	}

	percent = (float)ps.states / (float)pl.limit * 100.0;

	if (ps.running != 1) {
		(void)printf("PF CRITICAL - status: Disabled\n");
		return (STATE_CRITICAL);
	}

	if (ps.states >= states_critical) {
		(void)printf("PF CRITICAL - states: %u (%.1f%% - limit: %u)\n",
		    ps.states, percent, pl.limit);
		return (STATE_CRITICAL);
	}
	
	if (ps.states >= states_warning) {
		(void)printf("PF WARNING - states: %u (%.1f%% - limit: %u)\n",
		    ps.states, percent, pl.limit);
		return (STATE_WARNING);
	}

	(void)printf("PF OK - states: %u (%.1f%% - limit: %u)\n",
	    ps.states, percent, pl.limit);
	return (STATE_OK);
}

static void
version(void)
{
	(void)fprintf(stderr, "%s %s\n", __progname, revision);
}

static void
usage(void)
{
	(void)fprintf(stderr, "Usage: %s [-Vh] [-w number] [-c number]\n",
	    __progname);
	(void)fprintf(stderr, "        ");
	(void)fprintf(stderr, "-V        - Print the plugin version\n");
	(void)fprintf(stderr, "        ");
	(void)fprintf(stderr, "-h        - Print the plugin help\n");
	(void)fprintf(stderr, "        ");
	(void)fprintf(stderr, "-w number - Warning when <number> states"
	    " (default: %u%% of state limit)\n", DEFAULT_WARN_PERCENT);
	(void)fprintf(stderr, "        ");
	(void)fprintf(stderr, "-c number - Critical when <number> states"
	    " (default: %u%% of state limit)\n", DEFAULT_CRIT_PERCENT);
	exit(EXIT_FAILURE);
}

static void
help(void)
{
	version();
	(void)fprintf(stderr, "\n");
	(void)fprintf(stderr, "This plugin checks if PF is enabled, and if ");
	(void)fprintf(stderr, "it is, the number of states\n");
	(void)fprintf(stderr, "currently in the state table.\n");
	(void)fprintf(stderr, "\n");
	(void)fprintf(stderr, "The current state count is compared to the ");
	(void)fprintf(stderr, "given (or default) thresholds and\n");
	(void)fprintf(stderr, "the proper Nagios state value is returned.\n");
	(void)fprintf(stderr, "\n");
	usage();
}
