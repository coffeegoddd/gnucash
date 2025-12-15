/***********************************************************************\
 * gnc-versioned-backend.hpp: Interface for versioned SQL backends       *
 *                                                                       *
 * Copyright 2025 Your Name                                              *
 *                                                                       *
 * This program is free software; you can redistribute it and/or         *
 * modify it under the terms of the GNU General Public License as        *
 * published by the Free Software Foundation; either version 2 of        *
 * the License, or (at your option) any later version.                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, contact:                             *
 *                                                                       *
 * Free Software Foundation           Voice:  +1-617-542-5942            *
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652            *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                        *
 ***********************************************************************/

#ifndef __GNC_VERSIONED_BACKEND_HPP__
#define __GNC_VERSIONED_BACKEND_HPP__

#include <string>
#include <vector>

/**
 * @brief Mixin interface for SQL backends that provide versioning
 *        and branching semantics, such as Dolt.
 *
 * This interface is intended to be implemented alongside GncSqlBackend
 * (typically via multiple inheritance) by backends that can expose
 * database-native history, branches, and commit operations.
 *
 * It deliberately uses only std:: types so that it can be included
 * from both engine and backend code without introducing extra
 * dependencies.
 */
class GncVersionedSqlBackend
{
public:
    virtual ~GncVersionedSqlBackend() = default;

    /**
     * @return true if the backend supports Dolt-style features
     *         (branches and commits). This allows future extension
     *         to other versioned backends.
     */
    virtual bool supports_dolt_features() const noexcept = 0;

    /**
     * List available branches in the underlying store.
     *
     * @param error_out Populated with a human-readable error message
     *                  on failure; left untouched on success.
     * @return A vector of branch names on success. Implementations
     *         should return an empty vector on error and set error_out.
     */
    virtual std::vector<std::string>
    dolt_list_branches(std::string& error_out) = 0;

    /**
     * Create a new branch in the underlying store without checking it out.
     *
     * Implementations should create the branch from the current HEAD
     * (or equivalent) unless extended later to support specifying a
     * different starting point.
     *
     * @param branch Name of the branch to create.
     * @param error_out Populated with a human-readable error message
     *                  on failure; left untouched on success.
     * @return true on success, false on failure.
     */
    virtual bool
    dolt_create_branch(const std::string& branch,
                       std::string& error_out) = 0;

    /**
     * Checkout a branch in the underlying store.
     *
     * Implementations should ensure that the current book state is
     * compatible with a branch switch (e.g. no unsaved changes) and
     * refresh any in-memory state as needed.
     *
     * @param branch Name of the branch to checkout.
     * @param error_out Populated with a human-readable error message
     *                  on failure; left untouched on success.
     * @return true on success, false on failure.
     */
    virtual bool
    dolt_checkout_branch(const std::string& branch,
                         std::string& error_out) = 0;

    /**
     * Stage changes in the underlying store in preparation for a commit.
     *
     * For Dolt this is conceptually equivalent to \"dolt add\"; the
     * initial implementation is expected to stage all changes in the
     * working set. Future versions may add more granular control if
     * needed.
     *
     * @param error_out Populated with a human-readable error message
     *                  on failure; left untouched on success.
     * @return true on success, false on failure.
     */
    virtual bool
    dolt_add(std::string& error_out) = 0;

    /**
     * Create a Dolt commit from the current working set.
     *
     * @param message Commit message.
     * @param author  Author name (may be empty to use a default).
     * @param email   Author email (may be empty to use a default).
     * @param new_commit_hash_out Populated with the new commit hash
     *                            on success; left untouched on failure.
     * @param error_out Populated with a human-readable error message
     *                  on failure; left untouched on success.
     * @return true on success, false on failure.
     */
    virtual bool
    dolt_commit(const std::string& message,
                const std::string& author,
                const std::string& email,
                std::string& new_commit_hash_out,
                std::string& error_out) = 0;
};

#endif // __GNC_VERSIONED_BACKEND_HPP__
