/********************************************************************\
 * gnc-backend-dolt.hpp: Dolt-backed SQL backend                     *
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

#ifndef GNC_BACKEND_DOLT_HPP
#define GNC_BACKEND_DOLT_HPP

#include "gnc-backend-dbi.hpp"
#include "gnc-versioned-backend.hpp"

/**
 * @brief Dolt-backed SQL backend.
 *
 * This backend mirrors the behaviour of the MySQL DBI backend but
 * adds Dolt-specific capabilities such as branches and commits.
 * It is intended to be used via a QofBackendProvider registered
 * under the access method "dolt" so that URIs like
 *   dolt://user:password@host/dbname
 * select this backend.
 */
class GncDoltBackend
    : public GncDbiBackend<DbType::DBI_MYSQL>,
      public GncVersionedSqlBackend
{
public:
    GncDoltBackend(GncSqlConnection* conn, QofBook* book);
    ~GncDoltBackend() override = default;

    /* QofBackend overrides */
    void safe_sync(QofBook* book) override;

    /* GncVersionedSqlBackend implementation */
    bool supports_dolt_features() const noexcept override { return true; }

    std::vector<std::string>
    dolt_list_branches(std::string& error_out) override;

    bool
    dolt_create_branch(const std::string& branch,
                       std::string& error_out) override;

    bool
    dolt_checkout_branch(const std::string& branch,
                         std::string& error_out) override;

    bool
    dolt_add(std::string& error_out) override;

    bool
    dolt_commit(const std::string& message,
                const std::string& author,
                const std::string& email,
                std::string& new_commit_hash_out,
                std::string& error_out) override;

private:
    /* Name of the Dolt branch that this backend has explicitly been
     * switched to via dolt_checkout_branch(). When empty no branch has
     * been selected and Dolt-specific write operations (safe_sync,
     * dolt_add, dolt_commit) must refuse to proceed.
     */
    std::string m_current_branch;

    bool execute_dolt_call(const std::string& sql,
                           std::string& error_out);
};

#endif // GNC_BACKEND_DOLT_HPP
