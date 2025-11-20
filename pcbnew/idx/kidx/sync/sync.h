/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2025 KiCad Developers, see AUTHORS.txt for contributors.
 * @author Fabien Corona <f.corona<at>laposte.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <string>
#include <vector>
#include <memory>

class KIDX_REGISTRY;

class IDX_FILE_REGISTRY
{
public:
    enum class STATUS
    {
        IDX_UNPROCESSED,
        IDX_PROCESSED,
        IDX_PROCESSED_DELETED
    };

    IDX_FILE_REGISTRY() {};

    virtual bool Load( const std::string& aFile ) { return false; };

    bool operator==( const IDX_FILE_REGISTRY& other ) const
    {
        return m_author == other.m_author && m_date == other.m_date && m_name == other.m_name;
    }

    bool operator!=( const IDX_FILE_REGISTRY& other ) const { return !( *this == other ); }

    const std::string GetAuthor() const { return m_author; };
    const std::string GetDate() const { return m_date; };
    const std::string GetName() const { return m_name; };
    const std::string GetPath() const { return m_path; };
    const std::string GetSoft() const { return m_soft; };
    const STATUS      GetStatus() const { return m_status; };
    const bool        IsBaseline() const { return m_baseline; };
    const bool        IsProbablyNext() const { return m_next; };

    friend class KIDX_REGISTRY;


protected:
    std::string m_author;
    std::string m_date;
    std::string m_name;
    std::string m_path;
    std::string m_soft;
    bool        m_baseline;
    bool        m_next;

    STATUS m_status;
};


class KIDX_REGISTRY
{
public:
    KIDX_REGISTRY();
    bool IsFileKnown( const std::shared_ptr<IDX_FILE_REGISTRY> aFile ) const;

    void ComputeNext();

    std::vector<std::shared_ptr<IDX_FILE_REGISTRY>> GetFiles() const { return m_files; };
    bool                                            AddFile( const std::shared_ptr<IDX_FILE_REGISTRY> aFile );
    std::string                                     GetPath() const { return m_path; };
    std::string                                     GetCollaborationFolder() const { return m_collaborationFolder; };
    bool                                            SetCollaborationFolder( std::string aPath )
    {
        m_collaborationFolder = aPath;
        return true;
    };

protected:
    std::vector<std::shared_ptr<IDX_FILE_REGISTRY>> m_files;
    std::string                                     m_path;
    std::string                                     m_collaborationFolder;
};
