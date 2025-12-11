/********************************************************************\
 * gnc-backend-dolt.h: C API for Dolt-backed SQL backend            *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652       *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                   *
 ********************************************************************/

#ifndef GNC_BACKEND_DOLT_H
#define GNC_BACKEND_DOLT_H

#include <glib.h>
#include <qof.h>

G_BEGIN_DECLS

/**
 * @brief Return TRUE if the backend is a Dolt-capable backend.
 */
gboolean gnc_dolt_backend_is_dolt(QofBackend* be);

/**
 * @brief List available Dolt branches.
 *
 * Returns a NULL-terminated array of newly-allocated strings on
 * success, or NULL on error. The caller is responsible for freeing
 * each string with g_free() and the array itself with g_free().
 */
gchar** gnc_dolt_list_branches(QofBackend* be);

/**
 * @brief Create a new branch without checking it out.
 */
gboolean gnc_dolt_create_branch(QofBackend* be, const gchar* branch);

/**
 * @brief Check out an existing branch.
 */
gboolean gnc_dolt_checkout_branch(QofBackend* be, const gchar* branch);

/**
 * @brief Stage changes in the working set for commit.
 */
gboolean gnc_dolt_add(QofBackend* be);

/**
 * @brief Create a Dolt commit from the current working set.
 *
 * On success, if out_commit_hash is non-NULL, it will be set to a
 * newly-allocated string containing the new commit hash (if
 * available). The caller is responsible for freeing it with g_free().
 */
gboolean gnc_dolt_commit(QofBackend* be,
                         const gchar* message,
                         const gchar* author,
                         const gchar* email,
                         gchar** out_commit_hash);

/**
 * @brief Convenience helper to open a QofSession on a specific Dolt branch.
 *
 * This function combines qof_session_begin(), an optional Dolt branch
 * checkout, and qof_session_load() into a single call. If @a branch is
 * non-NULL and non-empty the backend must be Dolt-capable or the call
 * will fail.
 *
 * On failure, the underlying session and/or backend will have recorded
 * an appropriate QofBackendError which can be retrieved with
 * qof_session_get_error().
 *
 * @param session         Session to open.
 * @param uri             Backend URI to open (e.g. dolt://...).
 * @param branch          Optional Dolt branch name; if NULL or empty no
 *                        branch checkout is performed.
 * @param mode            Session open mode (see SessionOpenMode).
 * @param percentage_func Optional progress callback for qof_session_load().
 *
 * @return TRUE on success, FALSE on failure.
 */
gboolean gnc_dolt_session_open_on_branch(QofSession *session,
                                         const gchar *uri,
                                         const gchar *branch,
                                         SessionOpenMode mode,
                                         QofPercentageFunc percentage_func);

/**
 * @brief Switch an existing session to a different Dolt branch.
 *
 * This helper ends the current session, reopens it on the same URI
 * with the requested @a mode, checks out the requested Dolt @a branch
 * on the new backend, and then loads the book.
 *
 * If the current session's book has unsaved changes the function will
 * fail without modifying the session. On any other failure the session
 * may have been ended or reopened but not loaded; callers should check
 * qof_session_get_error() and decide whether to retry or destroy the
 * session.
 *
 * @param session         Existing session to retarget.
 * @param branch          Dolt branch name to check out.
 * @param mode            Session open mode for the reopened session.
 * @param percentage_func Optional progress callback for qof_session_load().
 *
 * @return TRUE on success, FALSE on failure.
 */
gboolean gnc_dolt_session_checkout_branch(QofSession *session,
                                          const gchar *branch,
                                          SessionOpenMode mode,
                                          QofPercentageFunc percentage_func);

G_END_DECLS

#endif /* GNC_BACKEND_DOLT_H */
