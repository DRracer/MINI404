/*
 * GLIB Compatibility Functions
 *
 * Copyright IBM, Corp. 2013
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *  Michael Tokarev   <mjt@tls.msk.ru>
 *  Paolo Bonzini     <pbonzini@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef QEMU_GLIB_COMPAT_H
#define QEMU_GLIB_COMPAT_H

/* Ask for warnings for anything that was marked deprecated in
 * the defined version, or before. It is a candidate for rewrite.
 */
#define GLIB_VERSION_MIN_REQUIRED GLIB_VERSION_2_64

/* Ask for warnings if code tries to use function that did not
 * exist in the defined version. These risk breaking builds
 */
#define GLIB_VERSION_MAX_ALLOWED GLIB_VERSION_2_64

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <glib.h>
#if defined(G_OS_UNIX)
#include <glib-unix.h>
#include <sys/types.h>
#include <pwd.h>
#endif

/*
 * These functions perform function pointer casts which can cause function call
 * failure on Emscripten. Use g_slist_sort_with_data and g_list_sort_with_data
 * instead of these functions.
 */
#pragma GCC poison g_slist_sort g_list_sort

/*
 * Note that because of the GLIB_VERSION_MAX_ALLOWED constant above, allowing
 * use of functions from newer GLib via this compat header needs a little
 * trickery to prevent warnings being emitted.
 *
 * Consider a function from newer glib-X.Y that we want to use
 *
 *    int g_foo(const char *wibble)
 *
 * We must define a static inline function with the same signature that does
 * what we need, but with a "_compat" suffix e.g.
 *
 * static inline void g_foo_compat(const char *wibble)
 * {
 *     #if GLIB_CHECK_VERSION(X, Y, 0)
 *        g_foo(wibble)
 *     #else
 *        g_something_equivalent_in_older_glib(wibble);
 *     #endif
 * }
 *
 * The #pragma at the top of this file turns off -Wdeprecated-declarations,
 * ensuring this wrapper function impl doesn't trigger the compiler warning
 * about using too new glib APIs. Finally we can do
 *
 *   #define g_foo(a) g_foo_compat(a)
 *
 * So now the code elsewhere in QEMU, which *does* have the
 * -Wdeprecated-declarations warning active, can call g_foo(...) as normal,
 * without generating warnings.
 */

/*
 * g_memdup2_qemu:
 * @mem: (nullable): the memory to copy.
 * @byte_size: the number of bytes to copy.
 *
 * Allocates @byte_size bytes of memory, and copies @byte_size bytes into it
 * from @mem. If @mem is %NULL it returns %NULL.
 *
 * This replaces g_memdup(), which was prone to integer overflows when
 * converting the argument from a #gsize to a #guint.
 *
 * This static inline version is a backport of the new public API from
 * GLib 2.68, kept internal to GLib for backport to older stable releases.
 * See https://gitlab.gnome.org/GNOME/glib/-/issues/2319.
 *
 * Returns: (nullable): a pointer to the newly-allocated copy of the memory,
 *          or %NULL if @mem is %NULL.
 */
static inline gpointer g_memdup2_qemu(gconstpointer mem, gsize byte_size)
{
#if GLIB_CHECK_VERSION(2, 68, 0)
    return g_memdup2(mem, byte_size);
#else
    gpointer new_mem;

    if (mem && byte_size != 0) {
        new_mem = g_malloc(byte_size);
        memcpy(new_mem, mem, byte_size);
    } else {
        new_mem = NULL;
    }

    return new_mem;
#endif
}
#define g_memdup2(m, s) g_memdup2_qemu(m, s)

static inline bool
qemu_g_test_slow(void)
{
    static int cached = -1;
    if (cached == -1) {
        cached = g_test_slow() || getenv("G_TEST_SLOW") != NULL;
    }
    return cached;
}

#undef g_test_slow
#undef g_test_thorough
#undef g_test_quick
#define g_test_slow() qemu_g_test_slow()
#define g_test_thorough() qemu_g_test_slow()
#define g_test_quick() (!qemu_g_test_slow())

/*
 * GUri compat for glib < 2.66.
 *
 * GUri was added in glib 2.66. QEMU 9.2 uses it in block drivers (nbd, ssh,
 * nfs, gluster) for URI parsing. On older glib we provide a minimal shim
 * backed by basic string parsing — sufficient for the URI forms QEMU uses.
 */
#if !GLIB_CHECK_VERSION(2, 66, 0)

#include <string.h>
#include <stdlib.h>

typedef struct _QemuGUri {
    char *scheme;
    char *host;
    char *path;
    char *query;
    char *user;
    int port;
} QemuGUri;

#define GUri QemuGUri
#define G_URI_FLAGS_NONE 0
#define G_URI_PARAMS_NONE 0

static inline void qemu_g_uri_free(QemuGUri *uri)
{
    if (uri) {
        g_free(uri->scheme);
        g_free(uri->host);
        g_free(uri->path);
        g_free(uri->query);
        g_free(uri->user);
        g_free(uri);
    }
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(QemuGUri, qemu_g_uri_free)

/*
 * Minimal URI parser for scheme://[user@]host[:port][/path][?query]
 */
static inline QemuGUri *g_uri_parse(const char *uri_str, int flags,
                                    GError **error)
{
    QemuGUri *uri;
    const char *p, *at, *colon, *slash, *qmark;

    (void)flags;

    p = strstr(uri_str, "://");
    if (!p) {
        if (error) {
            *error = g_error_new_literal(G_MARKUP_ERROR, 0, "no scheme");
        }
        return NULL;
    }

    uri = g_new0(QemuGUri, 1);
    uri->scheme = g_strndup(uri_str, p - uri_str);
    uri->port = -1;
    p += 3; /* skip :// */

    /* find end of authority (next / or ? or end) */
    slash = strchr(p, '/');
    qmark = strchr(p, '?');
    const char *auth_end = slash ? slash : (qmark ? qmark : p + strlen(p));

    /* user@host or just host */
    char *authority = g_strndup(p, auth_end - p);
    at = strchr(authority, '@');
    const char *hoststart;
    if (at) {
        uri->user = g_strndup(authority, at - authority);
        hoststart = at + 1;
    } else {
        hoststart = authority;
    }

    /* host[:port] — handle [ipv6] brackets */
    if (hoststart[0] == '[') {
        const char *bracket = strchr(hoststart, ']');
        if (bracket) {
            uri->host = g_strndup(hoststart + 1, bracket - hoststart - 1);
            if (bracket[1] == ':') {
                uri->port = atoi(bracket + 2);
            }
        } else {
            uri->host = g_strdup(hoststart + 1);
        }
    } else {
        colon = strchr(hoststart, ':');
        if (colon) {
            uri->host = g_strndup(hoststart, colon - hoststart);
            uri->port = atoi(colon + 1);
        } else {
            uri->host = g_strdup(hoststart);
        }
    }
    g_free(authority);

    /* path */
    if (slash) {
        if (qmark && qmark > slash) {
            uri->path = g_strndup(slash, qmark - slash);
        } else {
            uri->path = g_strdup(slash);
        }
    } else {
        uri->path = g_strdup("");
    }

    /* query */
    if (qmark) {
        uri->query = g_strdup(qmark + 1);
    }

    return uri;
}

static inline const char *g_uri_get_scheme(QemuGUri *uri)
{
    return uri->scheme;
}

static inline const char *g_uri_get_host(QemuGUri *uri)
{
    return uri->host;
}

static inline const char *g_uri_get_path(QemuGUri *uri)
{
    return uri->path;
}

static inline const char *g_uri_get_query(QemuGUri *uri)
{
    return uri->query;
}

static inline const char *g_uri_get_user(QemuGUri *uri)
{
    return uri->user;
}

static inline int g_uri_get_port(QemuGUri *uri)
{
    return uri->port;
}

/*
 * Minimal query parameter parser: split "key=val&key2=val2" into a hash table.
 */
static inline GHashTable *g_uri_parse_params(const char *params, gssize length,
                                             const char *separators, int flags,
                                             GError **error)
{
    GHashTable *ht;
    char *copy, *saveptr = NULL, *token;

    (void)flags;
    (void)error;

    if (!params) {
        return NULL;
    }

    ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    copy = (length < 0) ? g_strdup(params) : g_strndup(params, length);

    for (token = strtok_r(copy, separators, &saveptr);
         token;
         token = strtok_r(NULL, separators, &saveptr)) {
        char *eq = strchr(token, '=');
        if (eq) {
            g_hash_table_insert(ht, g_strndup(token, eq - token),
                                g_strdup(eq + 1));
        } else {
            g_hash_table_insert(ht, g_strdup(token), g_strdup(""));
        }
    }
    g_free(copy);
    return ht;
}

/*
 * GUriParamsIter compat — used by ssh.c and nfs.c (only compiled when
 * libssh/libnfs are found, which they aren't on this system, but provide
 * it for completeness).
 */
typedef struct {
    GHashTableIter iter;
} GUriParamsIter;

static inline void g_uri_params_iter_init(GUriParamsIter *qp,
                                          const char *params, gssize length,
                                          const char *separators, int flags)
{
    /* This is a stub — the real iter would parse on-the-fly, but the callers
     * using this are in block drivers that won't be compiled. */
    (void)qp; (void)params; (void)length; (void)separators; (void)flags;
}

#endif /* !GLIB_CHECK_VERSION(2, 66, 0) */

#pragma GCC diagnostic pop

#ifndef G_NORETURN
#define G_NORETURN G_GNUC_NORETURN
#endif

#endif
