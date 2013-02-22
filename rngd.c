/*
 * rngd.c -- Random Number Generator daemon
 *
 * rngd reads data from a hardware random number generator, verifies it
 * looks like random data, and adds it to /dev/random's entropy store.
 *
 * In theory, this should allow you to read very quickly from
 * /dev/random; rngd also adds bytes to the entropy store periodically
 * when it's full, which makes predicting the entropy store's contents
 * harder.
 *
 * Copyright (C) 2001 Philipp Rumpf
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335  USA
 */

#define _GNU_SOURCE

#ifndef HAVE_CONFIG_H
#error Invalid or missing autoconf build environment
#endif

#include "rng-tools-config.h"

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <syslog.h>
#include <signal.h>

#include "rngd.h"
#include "fips.h"
#include "exits.h"
#include "rngd_entsource.h"
#include "rngd_linux.h"

/*
 * Globals
 */

/* Background/daemon mode */
bool am_daemon;				/* True if we went daemon */

bool server_running = true;		/* set to false, to stop daemon */

/* Command line arguments and processing */
const char *argp_program_version =
	"rngd " VERSION "\n"
	"Copyright 2001-2004 Jeff Garzik\n"
	"Copyright (c) 2001 by Philipp Rumpf\n"
	"This is free software; see the source for copying conditions.  There is NO "
	"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.";

const char *argp_program_bug_address = PACKAGE_BUGREPORT;

static char doc[] =
	"Check and feed random data from hardware device to kernel entropy pool.\n";

#ifndef _ARGP_H
struct argp_option {
	const char *name;
	int key;
	const char *arg;
	int flags;
	const char *doc;
	int group;
};
#endif

static struct argp_option options[] = {
	{ "foreground",	'f', 0, 0, "Do not fork and become a daemon" },

	{ "background", 'b', 0, 0, "Become a daemon (default)" },

	{ "random-device", 'o', "file", 0,
	  "Kernel device used for random number output (default: /dev/random)" },

	{ "rng-device", 'r', "file", 0,
	  "Kernel device used for random number input (default: /dev/hwrng)" },

	{ "pid-file", 'p', "file", 0,
	  "File used for recording daemon PID, and multiple exclusion (default: /tmp/rngd.pid)" },

	{ "random-step", 's', "nnn", 0,
	  "Number of bytes written to random-device at a time (default: 64)" },

	{ "fill-watermark", 'W', "n", 0,
	  "Do not stop feeding entropy to random-device until at least n bits of entropy are available in the pool (default: 2048), 0 <= n <= 4096" },

	{ "quiet", 'q', 0, 0, "Suppress error messages" },

	{ "verbose" ,'v', 0, 0, "Report available entropy sources" },

	{ "no-drng", 'd', "1|0", 0,
	  "Do not use drng as a source of random number input (default: 0)" },
	
	{ "no-tpm", 'n', "1|0", 0,
	  "Do not use tpm as a source of random number input (default: 0)" },

	{ 0 },
};

static struct arguments default_arguments = {
	.random_name	= "/dev/random",
	// Android doesn't have /var/run, for now, /tmp is best place 
	// I did come up with
	.pid_file	= "/tmp/rngd.pid", 
	.random_step	= 64,
	.daemon		= true,
	.enable_drng	= true,
	.enable_tpm	= true,
	.quiet		= false,
	.verbose	= false,
};
struct arguments *arguments = &default_arguments;

static struct rng rng_default = {
	.rng_name	= "/dev/hwrng",
	.rng_fd		= -1,
	.xread		= xread,
};

static struct rng rng_drng = {
	.rng_name	= "drng",
	.rng_fd  	= -1,
	.xread  	= xread_drng,
};

static struct rng rng_tpm = {
	.rng_name	= "/dev/tpm0",
	.rng_fd		= -1,
	.xread		= xread_tpm,
};

struct rng *rng_list;

/*
 * command line processing
 */
static struct option long_options[] = {
	{"foreground",		no_argument, 		NULL, 'f' },
	{"background",		no_argument, 		NULL, 'b' },
	{"random-device", 	required_argument, 	NULL, 'o' },
	{"rng-device", 		required_argument, 	NULL, 'r' },
	{"pid-file", 		required_argument, 	NULL, 'p' },
	{"random-step", 	required_argument, 	NULL, 's' },
	{"fill-watermark", 	required_argument,	NULL, 'W' },
	{"quiet",		no_argument,		NULL, 'q' },
	{"verbose",		no_argument,		NULL, 'v' },
	{"no-drng",		required_argument,	NULL, 'd' },
	{"no-tpm", 		required_argument,	NULL, 'n' },
	{"version", 		no_argument,		NULL, 'V' },
	{"help", 		no_argument,		NULL, 'h' },
	{0, 0, 0, 0 }
};

void opt_usage(int key) {
	int i = 0;
	for (; i < sizeof(options); i++) {
		if (options[i].name == (unsigned int) 0)
			break;

		if (key != 0 && options[i].key == key) {
			fputs(options[i].name, stderr);
			return;
		}
		else {
			printf("\t--%s,-%c\t%s\n", options[i].name, options[i].key, options[i].doc);
		}
	}
}

void print_doc() {
	puts(doc);
}

void print_version() {
	puts(argp_program_version);
}

static int parse_opts (int argc, char **argv) {
	int c;
	int option_index = 0;

	while (1) {
		c = getopt_long(argc, argv, "fbo:r:p:s:W:qvd:n:hV",
			long_options, &option_index);

		if (c == -1)
			break;

		switch(c) {
		case 'o':
			arguments->random_name = optarg;
			break;
		case 'p':
			arguments->pid_file = optarg;
			break;
		case 'r':
			rng_default.rng_name = optarg;
			break;
		case 'f':
			arguments->daemon = false;
			break;
		case 'b':
			arguments->daemon = true;
			break;
		case 's':
			if (sscanf(optarg, "%i", &arguments->random_step) == 0)
				opt_usage(c);
			break;
		case 'W': {
			int n;
			if ((sscanf(optarg, "%i", &n) == 0) || (n < 0) || (n > 4096))
				opt_usage(c);
			else
				arguments->fill_watermark = n;
			break;
		}
		case 'q':
			arguments->quiet = true;
			break;
		case 'v':
			arguments->verbose = true;
			break;
		case 'd': {
			int n;
			if ((sscanf(optarg, "%i", &n) == 0) || ((n | 1)!=1))
				opt_usage(c);
			else
				arguments->enable_drng = false;
			break;
		}
		case 'n': {
			int n;
			if ((sscanf(optarg, "%i", &n) == 0) || ((n | 1)!=1))
				opt_usage(c);
			else
				arguments->enable_tpm = false;
			break;
		}
		case 'V':
			print_version();
			exit(EXIT_SUCCESS);
		case 'h':
			print_doc();
			opt_usage(0);
			exit(EXIT_SUCCESS);
		case '?':
		default:
			return 1;
		}
	}
	return 0;
}

static int update_kernel_random(int random_step,
	unsigned char *buf, fips_ctx_t *fipsctx_in)
{
	unsigned char *p;
	int fips;

	fips = fips_run_rng_test(fipsctx_in, buf);
	if (fips)
		return 1;

	for (p = buf; p + random_step <= &buf[FIPS_RNG_BUFFER_SIZE];
		 p += random_step) {
		random_add_entropy(p, random_step);
		random_sleep();
	}
	return 0;
}

static void do_loop(int random_step)
{
	unsigned char buf[FIPS_RNG_BUFFER_SIZE];
	int retval = 0;
	int no_work = 0;

	while (no_work < 100) {
		struct rng *iter;
		bool work_done;

		work_done = false;
		for (iter = rng_list; iter; iter = iter->next)
		{
			int rc;

			if (!server_running)
				return;

		retry_same:
			if (iter->disabled)
				continue;	/* failed, no work */

			retval = iter->xread(buf, sizeof buf, iter);
			if (retval)
				continue;	/* failed, no work */

			work_done = true;

			rc = update_kernel_random(random_step,
					     buf, iter->fipsctx);
			if (rc == 0) {
				iter->success++;
				if (iter->success >= RNG_OK_CREDIT) {
					if (iter->failures)
						iter->failures--;
					iter->success = 0;
				}
				break;	/* succeeded, work done */
			}

			iter->failures++;
			if (iter->failures <= MAX_RNG_FAILURES/4) {
				/* FIPS tests have false positives */
				goto retry_same;
			} else if (iter->failures >= MAX_RNG_FAILURES) {
				if (!arguments->quiet)
					message(LOG_DAEMON|LOG_ERR,
					"too many FIPS failures, disabling entropy source\n");
				iter->disabled = true;
			}
		}

		if (!work_done)
			no_work++;
	}

	if (!arguments->quiet)
		message(LOG_DAEMON|LOG_ERR,
		"No entropy sources working, exiting rngd\n");
}

static void term_signal(int signo)
{
	server_running = false;
}

int main(int argc, char **argv)
{
	int rc_rng = 1;
	int rc_drng = 1;
	int rc_tpm = 1;
	int pid_fd = -1;

	openlog("rngd", 0, LOG_DAEMON);

	/* Get the default watermark level for this platform */
	arguments->fill_watermark = default_watermark();

	/* Parsing of commandline parameters */
	parse_opts(argc, argv);	

	/* Init entropy sources, and open TRNG device */
	if (arguments->enable_drng)
		rc_drng = init_drng_entropy_source(&rng_drng);
	rc_rng = init_entropy_source(&rng_default);
	if (arguments->enable_tpm && rc_rng)
		rc_tpm = init_tpm_entropy_source(&rng_tpm);

	if (rc_rng && rc_drng && rc_tpm) {
		if (!arguments->quiet) {
			message(LOG_DAEMON|LOG_ERR,
				"can't open any entropy source");
			message(LOG_DAEMON|LOG_ERR,
				"Maybe RNG device modules are not loaded\n");
		}
		return 1;
	}

	if (arguments->verbose) {
		printf("Available entropy sources:\n");
		if (!rc_rng)
			printf("\tIntel/AMD hardware rng\n");
		if (!rc_drng)
			printf("\tDRNG\n");
		if (!rc_tpm)
			printf("\tTPM\n");
	}

	if (rc_rng
		&& (rc_drng || !arguments->enable_drng)
		&& (rc_tpm || !arguments->enable_tpm)) {
		if (!arguments->quiet)
			message(LOG_DAEMON|LOG_ERR,
		"No entropy source available, shutting down\n");
		return 1;
	}

	/* Init entropy sink and open random device */
	init_kernel_rng(arguments->random_name);

	if (arguments->daemon) {
		am_daemon = true;

		// android devices don't have /dev/null device
		// we can either create it in init phase of phoneboot, or 
		// enable noclose on daemon(3). The noclose fix is more 
		// universal.
		if (daemon(0, 1) < 0) {
			if(!arguments->quiet)
				fprintf(stderr, "can't daemonize: %s\n",
				strerror(errno));
			return 1;
		}

		/* require valid, locked PID file to proceed */
		pid_fd = write_pid_file(arguments->pid_file);
		if (pid_fd < 0)
			return 1;

		signal(SIGHUP, SIG_IGN);
		signal(SIGPIPE, SIG_IGN);
		signal(SIGINT, term_signal);
		signal(SIGTERM, term_signal);
	}

	do_loop(arguments->random_step);

	if (pid_fd >= 0)
		unlink(arguments->pid_file);

	return 0;
}
