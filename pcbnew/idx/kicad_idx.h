/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2024 KiCad Developers, see AUTHORS.txt for contributors.
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


#ifndef KICAD_IDX_H
#define KICAD_IDX_H

#include <wx/xml/xml.h>
#include <board_commit.h>
#include <idx/kidx/kidx.h>
#include <board.h>
#include <widgets/report_severity.h>
#include <idx/idx_reader.h>
#include <unordered_map>

class LSET;
enum PCB_LAYER_ID : int;

namespace idx_pdm {
    class EDMD_FUNCTIONAL_ITEM_SHAPE;
    class EDMD_ITEM_INSTANCE;
}


class KICAD_IDX_CHANGE
{
public:
    enum class ELEMENT_TYPE
    {
        BOARD_OUTLINE = 0,
        BOARD_THICKNESS,
        RULE_ZONE,
        NPTH,
        PTH,
        KEEP,
        COMPONENT,
        OTHER
    };

    KICAD_IDX_CHANGE( IDX_BASE& aIdx ) :
            m_idx( aIdx ) {};


    ELEMENT_TYPE GetType( idx_pdm::EDMD_ITEM_INSTANCE& aInst );


    std::vector<BOARD_ITEM*> m_newItems;
    std::vector<BOARD_ITEM*> m_predecessorItems;
    ELEMENT_TYPE             m_type;
    wxString                 m_name;
    double                   m_value;
    bool                     baseline = true;

    // void Accept(){};
    // void Reject(){};

private:
    IDX_BASE& m_idx;
};


class KICAD_IDX
{
public:
    KICAD_IDX( std::shared_ptr<idx_foundation::EDMD_DATA_SET> aDataset, std::shared_ptr<KIDX_REGISTRY_KICAD> aReg,
               BOARD* aBoard, BOARD_COMMIT* aCommit );

    void                     AddSelected( std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>>& aVector );
    std::vector<BOARD_ITEM*> AddElement( idx_pdm::EDMD_ITEM_INSTANCE&, PCB_LAYER_ID );
    BOARD_ITEM*              AddElement( idx_pdm::EDMD_FUNCTIONAL_ITEM_SHAPE );
    void                     Apply( std::vector<std::shared_ptr<KICAD_IDX_CHANGE>> aVector );

    std::vector<BOARD_ITEM*> ConvertInstance( idx_pdm::EDMD_ITEM_INSTANCE& aInst, bool aPredecessor = false );

    std::shared_ptr<KICAD_IDX_CHANGE> ConvertChange( idx_computational::EDMD_CHANGE& aChange );

    // Convert IDX shapes to BOARD_ITEM*
    std::vector<BOARD_ITEM*> ConvertElement( idx_geometry::EDMD_CURVE_SET2D& );
    std::vector<BOARD_ITEM*> ConvertElement( idx_geometry::EDMD_ARC& );
    std::vector<BOARD_ITEM*> ConvertElement( idx_geometry::EDMD_CIRCLE& );
    std::vector<BOARD_ITEM*> ConvertElement( idx_geometry::EDMD_POLY_LINE& );

    // Abstraction
    std::vector<std::shared_ptr<KICAD_IDX_CHANGE>> Process();
    std::vector<std::shared_ptr<KICAD_IDX_CHANGE>> GetChanges();
    std::vector<std::shared_ptr<KICAD_IDX_CHANGE>> SendInformation();

    // Response file generation
    std::shared_ptr<idx_foundation::EDMD_DATA_SET> CreateSendInformationResponse();
    bool                                           SaveSendInformationResponse( const std::string& aFilename );

    bool CheckInstance( idx_pdm::EDMD_ITEM_INSTANCE& );
    bool ItemHasIdentifier( const idx_pdm::EDMD_ITEM& aItem, const idx_foundation::EDMD_IDENTIFIER& aId ) const;

    std::vector<BOARD_ITEM*> AddNPTH( idx_pdm::EDMD_ITEM_INSTANCE& aInstance );
    std::vector<BOARD_ITEM*> AddPTH( idx_pdm::EDMD_ITEM_INSTANCE& aInstance );
    std::vector<BOARD_ITEM*> AddZone( idx_pdm::EDMD_ITEM_INSTANCE& aInstance );

    void SetKeepRules( ZONE&, idx_pdm::EDMD_KEEP_CONSTRAINT& );

    bool ApplyTransformation( BOARD_ITEM* aItem, const idx_pdm::EDMD_TRANSFORMATION& aTransfo );


    void Report( wxString aMsg, SEVERITY aSev = RPT_SEVERITY_INFO );

    VECTOR2I  ArcMidPoint( idx_geometry::EDMD_ARC& );
    PCB_SHAPE ConvertToKiCadShape( idx_geometry::EDMD_ARC& );

    void AddItem( BOARD_ITEM*, LSET );
    LSET GetLayers( idx_geometry::EDMD_CURVE_SET2D& );

    std::shared_ptr<idx_foundation::EDMD_DATA_SET> m_dataset;

    BOARD_COMMIT*                             m_commit;
    BOARD*                                    m_board;
    std::vector<std::pair<PCB_LAYER_ID, int>> m_stackup;
    double                                    m_boardThickness;
    std::shared_ptr<KIDX_REGISTRY_KICAD>      m_reg;

    // Layer mappings for IDX types
    const std::unordered_map<int, PCB_LAYER_ID> m_idxShapeTypeToLayer;
    const std::unordered_map<int, PCB_LAYER_ID> m_idxInstanceTypeToLayer;
};


#endif
