/*
 * Copyright (c) 2015 Paul Fariello <paul@fariello.eu>
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

#include <getopt.h>
#include <gpgme.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "command.h"
#include "error.h"
#include "kickpass_config.h"
#include "log.h"
#include "storage.h"

/* commands */
#include "init.h"
#include "create.h"

static kp_error_t parse_opt(int argc, char **argv);
static int        cmd_cmp(const void *k, const void *e);
static kp_error_t command(int argc, char **argv);
static kp_error_t show_version(void);
static kp_error_t usage(void);

struct cmd {
	const char    *name;
	struct kp_cmd *cmd;
};

#define CMD_COUNT (sizeof(cmds)/sizeof(cmds[0]))

static struct cmd cmds[] = {
	/* kp_cmd_init */
	{ "init",   &kp_cmd_init },

	/* kp_cmd_create */
	{ "create", &kp_cmd_create },
	{ "new",    &kp_cmd_create },
	{ "insert", &kp_cmd_create },
};

int
main(int argc, char **argv)
{

	if (parse_opt(argc, argv) != KP_SUCCESS) error("cannot parse command line arguments");

	if (command(argc, argv) != KP_SUCCESS) {
		return EXIT_FAILURE;
	} else {
		return EXIT_SUCCESS;
	}
}

static kp_error_t
parse_opt(int argc, char **argv)
{
	int opt;
	static struct option longopts[] = {
		{ "version", no_argument, NULL, 'v' },
		{ "help",    no_argument, NULL, 'h' },
		{ NULL,      0,           NULL, 0   },
	};

	while ((opt = getopt_long(argc, argv, "vh", longopts, NULL)) != -1) {
		switch (opt) {
		case 'v':
			show_version();
			exit(EXIT_SUCCESS);
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		}
	}

	return KP_SUCCESS;
}

static int
cmd_cmp(const void *k, const void *e)
{
	return strcmp(k, ((struct cmd *)e)->name);
}

static kp_error_t
command(int argc, char **argv)
{
	const struct cmd *cmd;

	if (optind >= argc) {
		LOGE("missing command");
		return KP_EINPUT;
	}

	qsort(cmds, CMD_COUNT, sizeof(struct cmd), cmd_cmp);
	cmd = bsearch(argv[optind], cmds, CMD_COUNT, sizeof(struct cmd), cmd_cmp);

	optind++;

	if (!cmd) {
		LOGE("unknown command %s", argv[0]);
		return KP_EINPUT;
	}

	return cmd->cmd->main(argc, argv);
}

static kp_error_t
show_version(void)
{
	struct kp_storage_ctx *ctx;
	char storage_engine[10], storage_version[10];

	printf("KickPass version %d.%d.%d\n",
			KICKPASS_VERSION_MAJOR,
			KICKPASS_VERSION_MINOR,
			KICKPASS_VERSION_PATCH);

	kp_storage_init(&ctx);
	kp_storage_get_engine(ctx, storage_engine, sizeof(storage_engine));
	kp_storage_get_version(ctx, storage_version, sizeof(storage_version));
	printf("storage engine %s %s\n", storage_engine, storage_version);

	return KP_SUCCESS;
}

static kp_error_t
usage(void)
{
	int i;
	char usage[] =
		"usage: kickpass [-hv] <command> [<args>]\n"
		"\n"
		"options:\n"
		"    -h, --help     Display this help\n"
		"    -v, --version  Show kickass version\n"
		"\n"
		"commands:\n";
	printf(usage);
	for (i = 0; i < CMD_COUNT; i++) {
		if (cmds[i-1].cmd == cmds[i].cmd) continue;
		if (cmds[i].cmd->usage) cmds[i].cmd->usage();
	}

	return KP_SUCCESS;
}
