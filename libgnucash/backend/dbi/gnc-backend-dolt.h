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

G_END_DECLS

#endif /* GNC_BACKEND_DOLT_H */
