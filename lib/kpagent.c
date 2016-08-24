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

#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/socket.h>
#define __unused __attribute__((unused))
#include <sys/tree.h>
#include <sys/types.h>
#include <sys/un.h>

#include <assert.h>
#include <imsg.h>
#include <sodium.h>
#include <stdlib.h>
#include <unistd.h>

#include "kickpass.h"

#include "error.h"
#include "kpagent.h"

#define SOCKET_BACKLOG 128

struct kp_store {
	RB_ENTRY(kp_store) tree;
	struct kp_agent_safe *safe;
};

static int store_cmp(struct kp_store *, struct kp_store *);

RB_HEAD(storage, kp_store) storage = RB_INITIALIZER(&storage);
RB_PROTOTYPE_STATIC(storage, kp_store, tree, store_cmp);

kp_error_t
kp_agent_init(struct kp_agent *agent, const char *socket_path)
{
	assert(agent);

	agent->sock = -1;
	agent->connected = false;

	memset(&agent->sunaddr, 0, sizeof(struct sockaddr_un));
	agent->sunaddr.sun_family = AF_UNIX;
	if (strlcpy(agent->sunaddr.sun_path, socket_path, sizeof(agent->sunaddr.sun_path))
			>= sizeof(agent->sunaddr.sun_path)) {
		errno = ENAMETOOLONG;
		return KP_ERRNO;
	}

	if ((agent->sock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
		return KP_ERRNO;
	}

	imsg_init(&agent->ibuf, agent->sock);

	return KP_SUCCESS;
}

kp_error_t
kp_agent_connect(struct kp_agent *agent)
{
	assert(agent);

	if (connect(agent->sock, (struct sockaddr *)&agent->sunaddr,
				sizeof(struct sockaddr_un)) < 0) {
		return KP_ERRNO;
	}
	agent->connected = true;

	return KP_SUCCESS;
}

kp_error_t
kp_agent_listen(struct kp_agent *agent)
{
	assert(agent);

	if (bind(agent->sock, (struct sockaddr *)&agent->sunaddr,
				sizeof(struct sockaddr_un)) < 0) {
		return KP_ERRNO;
	}

	if (listen(agent->sock, SOCKET_BACKLOG) < 0) {
		return KP_ERRNO;
	}

	return KP_SUCCESS;
}

kp_error_t
kp_agent_send(struct kp_agent *agent, enum kp_agent_msg_type type, void *data,
              size_t size)
{
	assert(agent);

	imsg_compose(&agent->ibuf, KP_MSG_STORE, 1, 2, agent->sock, data, size);
	msgbuf_write(&agent->ibuf.w);

	return KP_SUCCESS;
}

kp_error_t
kp_agent_close(struct kp_agent *agent)
{
	imsg_clear(&agent->ibuf);

	if (agent->sock >= 0) {
		close(agent->sock);
	}

	return KP_SUCCESS;
}

kp_error_t
kp_agent_store(struct kp_agent *agent, struct kp_agent_safe *safe)
{
	struct kp_store *store;

	if ((store = malloc(sizeof(struct kp_store))) == NULL) {
		errno = ENOMEM;
		return KP_ERRNO;
	}

	store->safe = safe;

	RB_INSERT(storage, &storage, store);

	return KP_SUCCESS;
}

kp_error_t
kp_agent_safe_create(struct kp_agent *agent, struct kp_agent_safe **_safe)
{
	struct kp_agent_safe *safe;
	char **password;
	char **metadata;

	if ((safe = malloc(sizeof(struct kp_agent_safe))) == NULL) {
		errno = ENOMEM;
		return KP_ERRNO;
	}

	/* Store is the only able to set password and metadata memory */
	password = (char **)&safe->password;
	metadata = (char **)&safe->metadata;

	*password = sodium_malloc(KP_PASSWORD_MAX_LEN);
	*metadata = sodium_malloc(KP_METADATA_MAX_LEN);

	*_safe = safe;

	return KP_SUCCESS;
}

static int
store_cmp(struct kp_store *a, struct kp_store *b)
{
	return strncmp(a->safe->path, b->safe->path, PATH_MAX);
}

RB_GENERATE_STATIC(storage, kp_store, tree, store_cmp);
