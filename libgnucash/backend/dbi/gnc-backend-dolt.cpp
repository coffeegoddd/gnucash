/********************************************************************\
 * gnc-backend-dolt.cpp: Dolt-backed SQL backend implementation     *
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

#include <config.h>

#include <sstream>

#include <qof.h>

#include "gnc-backend-dolt.hpp"
#include "gnc-backend-dolt.h"
#include "gnc-sql-result.hpp"

GncDoltBackend::GncDoltBackend(GncSqlConnection* conn, QofBook* book)
    : GncDbiBackend<DbType::DBI_MYSQL>(conn, book)
{
}

bool
GncDoltBackend::execute_dolt_call(const std::string& sql,
                                  std::string& error_out)
{
    auto stmt = create_statement_from_sql(sql);
    if (!stmt)
    {
        if (error_out.empty())
            error_out = "Failed to create Dolt SQL statement";
        return false;
    }

    auto result = execute_nonselect_statement(stmt);
    if (result == -1)
    {
        if (error_out.empty())
            error_out = "Dolt SQL call failed";
        return false;
    }

    return true;
}

std::vector<std::string>
GncDoltBackend::dolt_list_branches(std::string& error_out)
{
    std::vector<std::string> branches;

    static const char* sql = "SELECT name FROM dolt_branches";
    auto stmt = create_statement_from_sql(sql);
    if (!stmt)
    {
        error_out = "Failed to create Dolt branch listing statement";
        return branches;
    }

    auto result = execute_select_statement(stmt);
    if (!result)
    {
        error_out = "Failed to execute Dolt branch listing statement";
        return branches;
    }

    for (auto row = result->begin(); row != result->end(); ++row)
    {
        auto name = row.get_string_at_col("name");
        if (name)
            branches.push_back(*name);
    }

    return branches;
}

bool
GncDoltBackend::dolt_create_branch(const std::string& branch,
                                   std::string& error_out)
{
    if (branch.empty())
    {
        error_out = "Branch name must not be empty";
        return false;
    }

    auto quoted_branch = quote_string(branch);
    std::ostringstream sql;
    // Dolt SQL procedure: CALL DOLT_BRANCH('<branch>');
    sql << "CALL DOLT_BRANCH(" << quoted_branch << ")";

    return execute_dolt_call(sql.str(), error_out);
}

bool
GncDoltBackend::dolt_checkout_branch(const std::string& branch,
                                     std::string& error_out)
{
    if (branch.empty())
    {
        error_out = "Branch name must not be empty";
        return false;
    }

    /*
     * Refuse to switch branches if there is an existing, unsaved book
     * attached to this backend. However, allow branch checkout before
     * any book has been loaded (m_book == nullptr) so that callers can
     * select a branch prior to the initial load.
     */
    if (m_book && qof_book_session_not_saved(m_book))
    {
        error_out = "Cannot checkout Dolt branch while book has unsaved changes";
        set_error(ERR_BACKEND_MISC);
        return false;
    }

    auto quoted_branch = quote_string(branch);
    std::ostringstream sql;
    // Dolt SQL procedure: CALL DOLT_CHECKOUT('<branch>');
    sql << "CALL DOLT_CHECKOUT(" << quoted_branch << ")";

    if (!execute_dolt_call(sql.str(), error_out))
        return false;

    /* Record the explicitly selected Dolt branch so that subsequent
     * Dolt write operations (safe_sync, dolt_add, dolt_commit) can
     * verify that a branch has been chosen. */
    m_current_branch = branch;

    /*
     * TODO: Consider reloading the book from the backend after the
     * branch switch so that in-memory state matches the checked-out
     * branch. For now we rely on callers to manage any necessary
     * reload.
     */
    return true;
}

bool
GncDoltBackend::dolt_add(std::string& error_out)
{
    if (m_current_branch.empty())
    {
        error_out = "Cannot stage changes: no Dolt branch has been "
                    "explicitly selected (call gnc_dolt_checkout_branch())";
        return false;
    }

    // Stage all changes in the working set; equivalent to `dolt add -A`
    // on the currently-selected Dolt branch.
    std::ostringstream sql;
    sql << "CALL DOLT_ADD(" << quote_string("-A") << ")";

    return execute_dolt_call(sql.str(), error_out);
}

bool
GncDoltBackend::dolt_commit(const std::string& message,
                            const std::string& author,
                            const std::string& email,
                            std::string& new_commit_hash_out,
                            std::string& error_out)
{
    if (m_current_branch.empty())
    {
        error_out = "Cannot create Dolt commit: no branch has been "
                    "explicitly selected (call gnc_dolt_checkout_branch())";
        return false;
    }

    if (message.empty())
    {
        error_out = "Commit message must not be empty";
        return false;
    }

    std::ostringstream sql;
    sql << "CALL DOLT_COMMIT(" << quote_string("-m")
        << ", " << quote_string(message);

    // Optionally add --author "Name <email>" if provided.
    if (!author.empty() || !email.empty())
    {
        std::string ident = author;
        if (!email.empty())
        {
            if (!ident.empty())
                ident += " ";
            ident += "<" + email + ">";
        }
        sql << ", " << quote_string("--author")
            << ", " << quote_string(ident);
    }

    // Close the CALL DOLT_COMMIT( ... ) invocation.
    sql << ")";

    if (!execute_dolt_call(sql.str(), error_out))
        return false;

    /*
     * Best-effort retrieval of the new commit hash. Dolt exposes a
     * `dolt_log` table with commit history; we assume a `commit_hash`
     * column ordered by date and take the latest entry.
     */
    static const char* log_sql =
        "SELECT commit_hash FROM dolt_log ORDER BY date DESC LIMIT 1";

    auto stmt = create_statement_from_sql(log_sql);
    if (!stmt)
    {
        // Commit likely succeeded; leave hash empty but don't treat as fatal.
        return true;
    }

    auto result = execute_select_statement(stmt);
    if (!result || result->size() == 0)
        return true; // Same reasoning as above.

    auto row = result->begin();
    auto hash = row.get_string_at_col("commit_hash");
    if (hash)
        new_commit_hash_out = *hash;

    return true;
}

void
GncDoltBackend::safe_sync(QofBook* book)
{
    /*
     * For Dolt-backed stores we want safe_sync() to guarantee that any
     * in-memory changes are both flushed to the SQL working set and
     * committed to the current Dolt branch.
     *
     * Earlier revisions of this backend assumed that callers such as
     * diffcash would always invoke Session.save() before safe_save(),
     * so the book was already synced and safe_sync() only needed to
     * perform DOLT_ADD/DOLT_COMMIT. That assumption doesn't hold for
     * generic callers that only ever use qof_session_safe_save(), in
     * which case changes might never be written to the database.
     *
     * To make safe_sync() self-contained while still avoiding the
     * MySQL-specific index juggling that Dolt may reject (see
     * GncDbiBackend<DbType::DBI_MYSQL>::safe_sync), we:
     *
     *   1. If the QofBook is dirty, flush it to the SQL working set
     *      using the generic SQL backend's sync() implementation.
     *   2. If auto-commit is enabled, stage and commit the resulting
     *      working set via DOLT_ADD + DOLT_COMMIT.
     *
     * This ensures that qof_session_safe_save() alone is sufficient
     * to persist and commit changes for Dolt-backed sessions, while
     * still allowing advanced callers to disable auto-commit and
     * manage Dolt commits explicitly.
     */

    /* Require callers to have explicitly selected a Dolt branch via
     * dolt_checkout_branch() before we touch the working set or create
     * commits. This avoids silently writing to whatever branch the
     * server considers current. */
    if (m_current_branch.empty())
    {
        set_error(ERR_BACKEND_MISC);
        set_message("Cannot perform safe_save on Dolt backend: no branch "
                    "has been explicitly selected (call gnc_dolt_checkout_branch())");
        return;
    }

    if (book && qof_book_session_not_saved(book))
    {
        /*
         * Flush any pending in-memory changes into the Dolt working
         * set using the generic SQL sync() path. We deliberately do
         * not delegate to the MySQL-safe safe_sync() implementation
         * here, as its index juggling can be rejected by Dolt.
         */
        GncSqlBackend::sync(book);

        if (check_error())
        {
            // Propagate SQL-level errors to the caller; do not attempt
            // to stage or commit via Dolt if the flush failed.
            return;
        }
    }

    // If auto-commit is disabled, do nothing; callers control commit behavior.
    if (!m_auto_commit)
        return;

    // Stage and commit all changes via Dolt.
    std::string error;
    if (!dolt_add(error))
    {
        if (!error.empty())
            set_message(std::move(error));
        // Mark an error but keep the DB changes; Dolt commit failed.
        set_error(ERR_BACKEND_SERVER_ERR);
        return;
    }

    std::string commit_hash;
    error.clear();

    auto author = m_default_author;
    auto email  = m_default_email;

    if (!dolt_commit("Automatic commit from GnuCash", author, email,
                     commit_hash, error))
    {
        if (!error.empty())
            set_message(std::move(error));
        set_error(ERR_BACKEND_SERVER_ERR);
        return;
    }
}

/* ===================================================================== */
/* C API wrappers                                                        */
/* ===================================================================== */

extern "C" {

static GncVersionedSqlBackend*
get_versioned_backend(QofBackend* be)
{
    if (!be)
        return nullptr;
    return dynamic_cast<GncVersionedSqlBackend*>(be);
}

gboolean
gnc_dolt_backend_is_dolt(QofBackend* be)
{
    auto vbe = get_versioned_backend(be);
    return vbe && vbe->supports_dolt_features();
}

gchar**
gnc_dolt_list_branches(QofBackend* be)
{
    auto vbe = get_versioned_backend(be);
    if (!vbe)
        return nullptr;

    std::string error;
    auto branches = vbe->dolt_list_branches(error);

    if (!error.empty() && be)
    {
        be->set_error(ERR_BACKEND_MISC);
        be->set_message(std::move(error));
    }

    if (branches.empty())
        return nullptr;

    auto count = branches.size();
    auto array = static_cast<gchar**>(g_new0(gchar*, count + 1));
    for (std::size_t i = 0; i < count; ++i)
        array[i] = g_strdup(branches[i].c_str());
    array[count] = nullptr;
    return array;
}

gboolean
gnc_dolt_create_branch(QofBackend* be, const gchar* branch)
{
    auto vbe = get_versioned_backend(be);
    if (!vbe || !branch)
        return FALSE;

    std::string error;
    if (!vbe->dolt_create_branch(branch, error))
    {
        if (!error.empty())
            be->set_message(std::move(error));
        be->set_error(ERR_BACKEND_MISC);
        return FALSE;
    }
    return TRUE;
}

gboolean
gnc_dolt_checkout_branch(QofBackend* be, const gchar* branch)
{
    auto vbe = get_versioned_backend(be);
    if (!vbe || !branch)
        return FALSE;

    std::string error;
    if (!vbe->dolt_checkout_branch(branch, error))
    {
        if (!error.empty())
            be->set_message(std::move(error));
        be->set_error(ERR_BACKEND_MISC);
        return FALSE;
    }
    return TRUE;
}

gboolean
gnc_dolt_add(QofBackend* be)
{
    auto vbe = get_versioned_backend(be);
    if (!vbe)
        return FALSE;

    std::string error;
    if (!vbe->dolt_add(error))
    {
        if (!error.empty())
            be->set_message(std::move(error));
        be->set_error(ERR_BACKEND_MISC);
        return FALSE;
    }
    return TRUE;
}

gboolean
gnc_dolt_commit(QofBackend* be,
                const gchar* message,
                const gchar* author,
                const gchar* email,
                gchar** out_commit_hash)
{
    auto vbe = get_versioned_backend(be);
    if (!vbe || !message)
        return FALSE;

    std::string error;
    std::string hash;
    std::string author_str = author ? author : "";
    std::string email_str  = email ? email : "";

    if (!vbe->dolt_commit(message, author_str, email_str, hash, error))
    {
        if (!error.empty())
            be->set_message(std::move(error));
        be->set_error(ERR_BACKEND_MISC);
        return FALSE;
    }

    if (out_commit_hash && !hash.empty())
        *out_commit_hash = g_strdup(hash.c_str());

    return TRUE;
}

} /* extern \"C\" */
