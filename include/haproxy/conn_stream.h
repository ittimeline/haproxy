/*
 * include/haproxy/conn_stream.h
 * This file contains conn-stream function prototypes
 *
 * Copyright 2021 Christopher Faulet <cfaulet@haproxy.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, version 2.1
 * exclusively.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _HAPROXY_CONN_STREAM_H
#define _HAPROXY_CONN_STREAM_H

#include <haproxy/api.h>
#include <haproxy/connection.h>
#include <haproxy/conn_stream-t.h>
#include <haproxy/obj_type.h>

struct buffer;
struct session;
struct appctx;
struct stream;
struct stream_interface;
struct check;

#define IS_HTX_CS(cs)     (cs_conn(cs) && IS_HTX_CONN(__cs_conn(cs)))

struct cs_endpoint *cs_endpoint_new();
void cs_endpoint_free(struct cs_endpoint *endp);

struct conn_stream *cs_new(struct cs_endpoint *endp);
struct conn_stream *cs_new_from_mux(struct cs_endpoint *endp, struct session *sess, struct buffer *input);
struct conn_stream *cs_new_from_applet(struct cs_endpoint *endp, struct session *sess, struct buffer *input);
struct conn_stream *cs_new_from_strm(struct stream *strm, unsigned int flags);
struct conn_stream *cs_new_from_check(struct check *check, unsigned int flags);
void cs_free(struct conn_stream *cs);

void cs_attach_mux(struct conn_stream *cs, void *target, void *ctx);
void cs_attach_applet(struct conn_stream *cs, void *target, void *ctx);
int cs_attach_strm(struct conn_stream *cs, struct stream *strm);

int cs_reset_endp(struct conn_stream *cs);
void cs_detach_endp(struct conn_stream *cs);
void cs_detach_app(struct conn_stream *cs);

/* Returns the endpoint target without any control */
static inline void *__cs_endp_target(const struct conn_stream *cs)
{
	return cs->endp->target;
}

/* Returns the endpoint context without any control */
static inline void *__cs_endp_ctx(const struct conn_stream *cs)
{
	return cs->endp->ctx;
}

/* Returns the connection from a cs if the endpoint is a mux stream. Otherwise
 * NULL is returned. __cs_conn() returns the connection without any control
 * while cs_conn() check the endpoint type.
 */
static inline struct connection *__cs_conn(const struct conn_stream *cs)
{
	return __cs_endp_ctx(cs);
}
static inline struct connection *cs_conn(const struct conn_stream *cs)
{
	if (cs->endp->flags & CS_EP_T_MUX)
		return __cs_conn(cs);
	return NULL;
}

/* Returns the mux ops of the connection from a cs if the endpoint is a
 * mux stream. Otherwise NULL is returned.
 */
static inline const struct mux_ops *cs_conn_mux(const struct conn_stream *cs)
{
	const struct connection *conn = cs_conn(cs);

	return (conn ? conn->mux : NULL);
}

/* Returns the mux from a cs if the endpoint is a mux. Otherwise
 * NULL is returned. __cs_mux() returns the mux without any control
 * while cs_mux() check the endpoint type.
 */
static inline void *__cs_mux(const struct conn_stream *cs)
{
	return __cs_endp_target(cs);
}
static inline struct appctx *cs_mux(const struct conn_stream *cs)
{
	if (cs->endp->flags & CS_EP_T_MUX)
		return __cs_mux(cs);
	return NULL;
}

/* Returns the appctx from a cs if the endpoint is an appctx. Otherwise
 * NULL is returned. __cs_appctx() returns the appctx without any control
 * while cs_appctx() check the endpoint type.
 */
static inline struct appctx *__cs_appctx(const struct conn_stream *cs)
{
	return __cs_endp_target(cs);
}
static inline struct appctx *cs_appctx(const struct conn_stream *cs)
{
	if (cs->endp->flags & CS_EP_T_APPLET)
		return __cs_appctx(cs);
	return NULL;
}

/* Returns the stream from a cs if the application is a stream. Otherwise
 * NULL is returned. __cs_strm() returns the stream without any control
 * while cs_strm() check the application type.
 */
static inline struct stream *__cs_strm(const struct conn_stream *cs)
{
	return __objt_stream(cs->app);
}
static inline struct stream *cs_strm(const struct conn_stream *cs)
{
	if (obj_type(cs->app) == OBJ_TYPE_STREAM)
		return __cs_strm(cs);
	return NULL;
}

/* Returns the healthcheck from a cs if the application is a
 * healthcheck. Otherwise NULL is returned. __cs_check() returns the healthcheck
 * without any control while cs_check() check the application type.
 */
static inline struct check *__cs_check(const struct conn_stream *cs)
{
	return __objt_check(cs->app);
}
static inline struct check *cs_check(const struct conn_stream *cs)
{
	if (obj_type(cs->app) == OBJ_TYPE_CHECK)
		return __objt_check(cs->app);
	return NULL;
}

/* Returns the stream-interface from a cs. It is not NULL only if a stream is
 * attached to the cs.
 */
static inline struct stream_interface *cs_si(const struct conn_stream *cs)
{
	return cs->si;
}

static inline const char *cs_get_data_name(const struct conn_stream *cs)
{
	if (!cs->data_cb)
		return "NONE";
	return cs->data_cb->name;
}

/* shut read */
static inline void cs_shutr(struct conn_stream *cs, enum co_shr_mode mode)
{
	const struct mux_ops *mux;

	if (!cs_conn(cs) || cs->endp->flags & CS_EP_SHR)
		return;

	/* clean data-layer shutdown */
	mux = cs_conn_mux(cs);
	if (mux && mux->shutr)
		mux->shutr(cs, mode);
	cs->endp->flags |= (mode == CO_SHR_DRAIN) ? CS_EP_SHRD : CS_EP_SHRR;
}

/* shut write */
static inline void cs_shutw(struct conn_stream *cs, enum co_shw_mode mode)
{
	const struct mux_ops *mux;

	if (!cs_conn(cs) || cs->endp->flags & CS_EP_SHW)
		return;

	/* clean data-layer shutdown */
	mux = cs_conn_mux(cs);
	if (mux && mux->shutw)
		mux->shutw(cs, mode);
	cs->endp->flags |= (mode == CO_SHW_NORMAL) ? CS_EP_SHWN : CS_EP_SHWS;
}

/* completely close a conn_stream (but do not detach it) */
static inline void cs_close(struct conn_stream *cs)
{
	cs_shutw(cs, CO_SHW_SILENT);
	cs_shutr(cs, CO_SHR_RESET);
}

/* completely close a conn_stream after draining possibly pending data (but do not detach it) */
static inline void cs_drain_and_close(struct conn_stream *cs)
{
	cs_shutw(cs, CO_SHW_SILENT);
	cs_shutr(cs, CO_SHR_DRAIN);
}

/* sets CS_EP_ERROR or CS_EP_ERR_PENDING on the cs */
static inline void cs_set_error(struct conn_stream *cs)
{
	if (cs->endp->flags & CS_EP_EOS)
		cs->endp->flags |= CS_EP_ERROR;
	else
		cs->endp->flags |= CS_EP_ERR_PENDING;
}

/* Retrieves any valid conn_stream from this connection, preferably the first
 * valid one. The purpose is to be able to figure one other end of a private
 * connection for purposes like source binding or proxy protocol header
 * emission. In such cases, any conn_stream is expected to be valid so the
 * mux is encouraged to return the first one it finds. If the connection has
 * no mux or the mux has no get_first_cs() method or the mux has no valid
 * conn_stream, NULL is returned. The output pointer is purposely marked
 * const to discourage the caller from modifying anything there.
 */
static inline const struct conn_stream *cs_get_first(const struct connection *conn)
{
	if (!conn || !conn->mux || !conn->mux->get_first_cs)
		return NULL;
	return conn->mux->get_first_cs(conn);
}

#endif /* _HAPROXY_CONN_STREAM_H */
