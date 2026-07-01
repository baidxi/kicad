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

#include "sync.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <unordered_set>
#include <algorithm>


KIDX_REGISTRY::KIDX_REGISTRY( const std::string& aFile ) :
        m_file( aFile ),
        m_identifierSet(),
        m_repository( std::make_shared<idx_foundation::EDMD_DATASET_BODY>() ),
        m_itemRepository(),
        m_baselined( false ),
        m_files(),
        m_path(),
        m_collaborationFolder(),
        m_identifiers()
{
    std::ifstream in( aFile );
    if( !in.good() )
    {
        std::cerr << "Failed to open IDX file: " << aFile << std::endl;
        return;
    }
    
    m_path = std::filesystem::path( aFile ).parent_path().string();
}

bool KIDX_REGISTRY::IsFileKnown( const std::shared_ptr<IDX_FILE_REGISTRY> aFile ) const
{
    if( !aFile )
        return false;

    for( const auto& file : m_files )
    {
        if( *file == *aFile )
            return true;
    }
    return false;
}


bool KIDX_REGISTRY::AddFile( const std::shared_ptr<IDX_FILE_REGISTRY> aFile )
{
    if( !aFile )
        return false;

    m_files.push_back( aFile );
    return true;
}


void KIDX_REGISTRY::ComputeNext()
{
    bool hasBaseline = false;
    std::shared_ptr<IDX_FILE_REGISTRY> latestNonBaseline = nullptr;
    std::shared_ptr<IDX_FILE_REGISTRY> firstBaseline = nullptr;
    
    for( auto& file : m_files )
    {
        if( file->IsBaseline() )
        {
            hasBaseline = true;
            if( !firstBaseline )
                firstBaseline = file;
        }
        else
        {
            if( !latestNonBaseline || file->GetDate() > latestNonBaseline->GetDate() )
                latestNonBaseline = file;
        }
    }
    
    // Reset all m_next flags
    for( auto& file : m_files )
        file->m_next = false;
    
    // Determine next file
    if( !hasBaseline && firstBaseline )
    {
        // No baseline exists yet, the first baseline is next
        firstBaseline->m_next = true;
    }
    else if( latestNonBaseline )
    {
        // We have baseline(s), the latest non-baseline (changeset) is next
        latestNonBaseline->m_next = true;
    }
}

void KIDX_REGISTRY::writeXml( std::shared_ptr<IDX_XML_NODE> aRoot )
{
    if( !aRoot )
        return;

    aRoot->SetAttribute( "version", "1.0" );


    auto child = aRoot->AppendChild( "folder" );
    child->SetAttribute( "path", this->GetPath() );

    for( auto file : m_files )
    {
        if( file->GetStatus() == IDX_FILE_REGISTRY::STATUS::IDX_UNPROCESSED )
            continue;

        auto child2 = aRoot->AppendChild( "file" );
        child2->SetAttribute( "name", file->GetName() );
        child2->SetAttribute( "author", file->GetAuthor() );
        child2->SetAttribute( "date", file->GetDate() );
        child2->SetAttribute( "software", file->GetSoft() );
    }

    auto ds = aRoot->AppendChild( "repository" );

    for( auto& item : m_itemRepository )
    {
        if( item.m_shape )
        {
            auto i = ds->AppendChild( "" );
            item.m_shape->Write( *i );
        }

        auto i = ds->AppendChild( "" );
        item.Write( *i );
    }
}

bool KIDX_REGISTRY::isIdentifierKnown( const idx_foundation::EDMD_IDENTIFIER& aId ) const
{
    // Create a unique key for the identifier
    std::string key = aId.m_systemScopeRef + ":" + aId.m_number;
    if( !aId.m_version.empty() )
        key += ":" + aId.m_version;
    if( !aId.m_revision.empty() )
        key += ":" + aId.m_revision;
    if( !aId.m_sequence.empty() )
        key += ":" + aId.m_sequence;
    
    return m_identifierSet.find( key ) != m_identifierSet.end();
}

void KIDX_REGISTRY::addIdentifier( const idx_foundation::EDMD_IDENTIFIER& aId )
{
    std::string key = aId.m_systemScopeRef + ":" + aId.m_number;
    if( !aId.m_version.empty() )
        key += ":" + aId.m_version;
    if( !aId.m_revision.empty() )
        key += ":" + aId.m_revision;
    if( !aId.m_sequence.empty() )
        key += ":" + aId.m_sequence;
    
    m_identifierSet.insert( key );
    m_identifiers.push_back( aId );
}


bool KIDX_REGISTRY::SC_Process( idx_computational::EDMD_CHANGE& aChange )
{
    // SC1: Check NewItem identifier
    if( !SC_1CheckIdentifierOfNewItem( aChange ) )
    {
        std::cerr << "SC1 failed: NewItem identifier unknown or missing" << std::endl;
        return false;
    }

    // SC2: Check PredecessorItem identifier
    if( !SC_2CheckIdentifierOfPredecessorItem( aChange ) )
    {
        // SC3: Request information for the predecessor
        std::cerr << "SC2 failed: PredecessorItem unknown, requesting information" << std::endl;
        SC_3RequestInformation( aChange );
        return false;
    }

    // SC4: Adjust item type if needed
    SC_4AdjustmentOfItemType();

    // Actually apply the change to the repository
    ApplyChangeToRepository( aChange );

    return true;
}

void KIDX_REGISTRY::SC_3RequestInformation( const idx_computational::EDMD_CHANGE& aChange )
{
    // TODO: Implement actual RequestForInformation message
    std::cerr << "SC3: Requesting information for predecessor: " 
              << (aChange.m_predecessorItem ? aChange.m_predecessorItem->m_number : "unknown") 
              << std::endl;
    // In a real implementation, this would:
    // 1. Create a RequestForInformation ProcessInstruction
    // 2. Send it to the source system
    // 3. Wait for response with the predecessor data
}

void KIDX_REGISTRY::SC_4AdjustmentOfItemType()
{
    // TODO: Implement item type adjustment logic
    // This would handle cases where the item type needs to be adjusted
    // based on the change context
}

bool KIDX_REGISTRY::SC_1CheckIdentifierOfNewItem( const idx_computational::EDMD_CHANGE& aChange ) const
{
    if( !aChange.m_newItem )
    {
        std::cerr << "SC1: NewItem is null" << std::endl;
        return false;
    }

    bool known = this->isIdentifierKnown( *( aChange.m_newItem ) );
    if( !known )
        std::cerr << "SC1: NewItem identifier unknown: " 
                  << aChange.m_newItem->m_systemScopeRef << ":" 
                  << aChange.m_newItem->m_number << std::endl;
    return known;
}

bool KIDX_REGISTRY::SC_2CheckIdentifierOfPredecessorItem( const idx_computational::EDMD_CHANGE& aChange ) const
{
    if( !aChange.m_predecessorItem )
    {
        // PredecessorItem is optional per spec, but if present, must be known
        return true;
    }

    bool known = this->isIdentifierKnown( *( aChange.m_predecessorItem ) );
    if( !known )
        std::cerr << "SC2: PredecessorItem identifier unknown: " 
                  << aChange.m_predecessorItem->m_systemScopeRef << ":" 
                  << aChange.m_predecessorItem->m_number << std::endl;
    return known;
}

void KIDX_REGISTRY::ApplyChangeToRepository( idx_computational::EDMD_CHANGE& aChange )
{
    std::cerr << "Applying change to repository" << std::endl;
    
    // TODO: Implement actual change application
    // This would:
    // 1. Find or create the item in m_itemRepository
    // 2. Apply the changes (add new item, update from predecessor, delete instances)
    // 3. Update identifiers in m_identifierSet
    
    if( aChange.m_newItem )
    {
        addIdentifier( *( aChange.m_newItem ) );
    }
    
    if( aChange.m_predecessorItem )
    {
        addIdentifier( *( aChange.m_predecessorItem ) );
    }
}

void KIDX_REGISTRY::MarkProcessed( std::shared_ptr<IDX_FILE_REGISTRY> aDataset )
{
    if( aDataset )
        aDataset->SetStatus( IDX_FILE_REGISTRY::STATUS::IDX_PROCESSED );
}

void KIDX_REGISTRY::Save()
{
    // TODO: Implement save to persistent storage
    std::cerr << "KIDX_REGISTRY::Save() - Not yet implemented" << std::endl;
}


// ============================================================================
// SendInformation Processing Methods (Developer Recommendations 6.4.2)
// ============================================================================

bool KIDX_REGISTRY::SI_1CheckIdentifiers( const idx_pdm::EDMD_ITEM& aItem ) const
{
    // Step 1: Checking identifiers (list)
    // a. If the item does not contain any identifiers, the message is errored
    if( aItem.m_identifier.empty() )
    {
        std::cerr << "SI_1a: Item has no identifiers - error" << std::endl;
        return false;
    }
    return true;
}


bool KIDX_REGISTRY::SI_2AdjustIdentifierRepository( idx_pdm::EDMD_ITEM& aItem )
{
    // Step 2: Adjustment of identifier repository
    for( const auto& identifier : aItem.m_identifier )
    {
        // a. If the identifiers contained in the item do not agree with the identifier repository
        if( isIdentifierKnown( identifier ) )
        {
            // Check if the identifier is consistent with repository
            // For now, we assume it agrees if it's known
            // In a full implementation, we'd need to check system scope, number, version, etc.
            continue;
        }
        else
        {
            // b. If the identifiers contained in the item do not disagree with the identifier repository
            // (i.e., they are new), store new identifiers in the repository
            addIdentifier( identifier );
        }
    }
    return true;
}


bool KIDX_REGISTRY::SI_3AdjustItemType( idx_pdm::EDMD_ITEM& aItem )
{
    // Step 3: Adjustment of ItemType
    // This step would compare the ItemType in the message with the existing representation
    // Since we don't have a way to look up existing items by identifier yet,
    // we'll skip the detailed comparison for now and just accept the message's ItemType

    // a. If the ItemType is not set in the existing representation of the item but is set in the message
    // b. If the ItemType is set in the existing representation of the item but is not set in the message
    // c. If the ItemType is set in the existing representation of the item and the same ItemType is set in the message
    // d. If the ItemType is set in the existing representation of the item but a different ItemType is set in the message
    // For now, we'll just accept whatever is in the message
    return true;
}


bool KIDX_REGISTRY::SI_4AdjustPackageNames( idx_pdm::EDMD_ITEM& aItem )
{
    // Step 4: Adjustment of PackageName, for every PackageName in the list
    // Similar to ItemType, we need to compare PackageNames between message and existing representation
    // For now, we'll accept whatever is in the message
    return true;
}


bool KIDX_REGISTRY::SI_5AdjustItemInstances( idx_pdm::EDMD_ITEM& aItem )
{
    // Step 5: Adjustment ItemInstance (list)
    // Compare ItemInstances between message and existing representation
    // For now, we'll accept whatever is in the message
    return true;
}


bool KIDX_REGISTRY::SI_6AdjustShape( idx_pdm::EDMD_ITEM& aItem )
{
    // Step 6: Adjustment Shape
    // Compare Shape between message and existing representation
    // For now, we'll accept whatever is in the message
    return true;
}


bool KIDX_REGISTRY::SI_7AdjustAcceptStatus( idx_pdm::EDMD_ITEM& aItem )
{
    // Step 7: Adjustment of Accept-Status, for every entry in the list
    // Compare Accept-Status between message and existing representation
    // For now, we'll accept whatever is in the message
    return true;
}


bool KIDX_REGISTRY::SI_8AdjustRoles( idx_pdm::EDMD_ITEM& aItem )
{
    // Step 8: Adjustment of the roles in EDMDItems and EDMDItemInstances
    // Note: roles and acceptance statuses are exceptions since they describe relationships
    // of individual actors to the state of the element, not the state itself
    // For now, we'll accept whatever is in the message
    return true;
}


bool KIDX_REGISTRY::SI_Process( idx_pdm::EDMD_ITEM& aItem )
{
    // SendInformation item processing according to Developer Recommendations 6.4.2
    // Process the 8 steps in order

    // Step 1: Checking identifiers (list)
    if( !SI_1CheckIdentifiers( aItem ) )
    {
        std::cerr << "SI_Process: Step 1 (CheckIdentifiers) failed for item " << aItem.m_id << std::endl;
        return false;
    }

    // Step 2: Adjustment of identifier repository
    if( !SI_2AdjustIdentifierRepository( aItem ) )
    {
        std::cerr << "SI_Process: Step 2 (AdjustIdentifierRepository) failed for item " << aItem.m_id << std::endl;
        return false;
    }

    // Step 3: Adjustment of ItemType
    if( !SI_3AdjustItemType( aItem ) )
    {
        std::cerr << "SI_Process: Step 3 (AdjustItemType) failed for item " << aItem.m_id << std::endl;
        return false;
    }

    // Step 4: Adjustment of PackageName
    if( !SI_4AdjustPackageNames( aItem ) )
    {
        std::cerr << "SI_Process: Step 4 (AdjustPackageNames) failed for item " << aItem.m_id << std::endl;
        return false;
    }

    // Step 5: Adjustment ItemInstance (list)
    if( !SI_5AdjustItemInstances( aItem ) )
    {
        std::cerr << "SI_Process: Step 5 (AdjustItemInstances) failed for item " << aItem.m_id << std::endl;
        return false;
    }

    // Step 6: Adjustment Shape
    if( !SI_6AdjustShape( aItem ) )
    {
        std::cerr << "SI_Process: Step 6 (AdjustShape) failed for item " << aItem.m_id << std::endl;
        return false;
    }

    // Step 7: Adjustment of Accept-Status
    if( !SI_7AdjustAcceptStatus( aItem ) )
    {
        std::cerr << "SI_Process: Step 7 (AdjustAcceptStatus) failed for item " << aItem.m_id << std::endl;
        return false;
    }

    // Step 8: Adjustment of the roles
    if( !SI_8AdjustRoles( aItem ) )
    {
        std::cerr << "SI_Process: Step 8 (AdjustRoles) failed for item " << aItem.m_id << std::endl;
        return false;
    }

    // All steps completed successfully
    return true;
}


idx_pdm::EDMD_ITEM* KIDX_REGISTRY::FindItemByIdentifier( const idx_foundation::EDMD_IDENTIFIER& aId )
{
    for( auto& item : m_itemRepository )
    {
        for( const auto& identifier : item.m_identifier )
        {
            if( aId == identifier ) // Uses EDMD_IDENTIFIER::operator==
                return &item;
        }
    }
    return nullptr;
}