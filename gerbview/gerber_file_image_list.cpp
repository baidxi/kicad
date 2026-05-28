/**
 * @file gerber_file_image.cpp
 * a GERBER class handle for a given layer info about used D_CODES and how the layer is drawn
 */

/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 1992-2016 Jean-Pierre Charras  jp.charras at wanadoo.fr
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
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

#include <gerbview.h>
#include <gerbview_frame.h>
#include <gerber_file_image.h>
#include <gerber_file_image_list.h>
#include <gerbview_settings.h>
#include <layer_roles.h>
#include <widgets/gerbview_layer_widget.h>
#include <X2_gerber_attributes.h>
#include <wx/filename.h>

#include <map>


// The global image list:
GERBER_FILE_IMAGE_LIST s_GERBER_List;


GERBER_FILE_IMAGE_LIST::GERBER_FILE_IMAGE_LIST()
{
    m_GERBER_List.reserve( GERBER_DRAWLAYERS_COUNT );

    for( unsigned layer = 0; layer < GERBER_DRAWLAYERS_COUNT; ++layer )
        m_GERBER_List.push_back( nullptr );
}


GERBER_FILE_IMAGE_LIST::~GERBER_FILE_IMAGE_LIST()
{
    DeleteAllImages();
}


GERBER_FILE_IMAGE_LIST& GERBER_FILE_IMAGE_LIST::GetImagesList()
{
    return s_GERBER_List;
}


GERBER_FILE_IMAGE* GERBER_FILE_IMAGE_LIST::GetGbrImage( int aIdx )
{
    if( (unsigned)aIdx < m_GERBER_List.size() )
        return m_GERBER_List[aIdx];

    return nullptr;
}


unsigned GERBER_FILE_IMAGE_LIST::GetLoadedImageCount()
{
    auto notNull = []( GERBER_FILE_IMAGE* image )
    {
        return image != nullptr;
    };
    return std::count_if( m_GERBER_List.begin(), m_GERBER_List.end(), notNull );
}


int GERBER_FILE_IMAGE_LIST::AddGbrImage( GERBER_FILE_IMAGE* aGbrImage, int aIdx )
{
    int idx = aIdx;

    if( idx < 0 )
    {
        for( idx = 0; idx < (int)m_GERBER_List.size(); idx++ )
        {
            if( m_GERBER_List[idx] == nullptr )
                break;
        }
    }

    if( idx >= (int)m_GERBER_List.size() )
        return -1;  // No room

    m_GERBER_List[idx] = aGbrImage;

    return idx;
}


void GERBER_FILE_IMAGE_LIST::DeleteAllImages()
{
    for( unsigned idx = 0; idx < m_GERBER_List.size(); ++idx )
        DeleteImage( idx );
}


void GERBER_FILE_IMAGE_LIST::DeleteImage( unsigned int aIdx )
{
    // Ensure the index is valid:
    if( aIdx >= m_GERBER_List.size() )
        return;

    // delete image aIdx
    GERBER_FILE_IMAGE* gbr_image = GetGbrImage( static_cast<int>( aIdx ) );

    delete gbr_image;
    m_GERBER_List[ aIdx ] = nullptr;
}


const wxString GERBER_FILE_IMAGE_LIST::GetDisplayName( int aIdx, bool aNameOnly, bool aFullName )
{
    wxString name;

    GERBER_FILE_IMAGE* gerber = nullptr;

    if( aIdx >= 0 && aIdx < (int)m_GERBER_List.size() )
        gerber = m_GERBER_List[aIdx];

    // if a file is loaded, build the name:
    // <id> <short filename> <X2 FileFunction info> if a X2 FileFunction info is found
    // or (if no FileFunction info)
    // <id> <short filename> *
    if( gerber )
    {
        wxFileName fn( gerber->m_FileName );
        wxString filename = fn.GetFullName();

        // If the filename is too long, display a shortened name if requested
        const int maxlen = 30;

        if( !aFullName && filename.Length() > maxlen )
        {
            wxString shortenedfn = filename.Left(2) + wxT( "..." ) + filename.Right(maxlen-5);
            filename = shortenedfn;
        }

        if( gerber->m_FileFunction )
        {
            if( gerber->m_FileFunction->IsCopper() )
            {
                name.Printf( wxT( "%s (%s, %s, %s)" ),
                             filename.GetData(),
                             gerber->m_FileFunction->GetFileType(),
                             gerber->m_FileFunction->GetBrdLayerId(),
                             gerber->m_FileFunction->GetBrdLayerSide() );
            }
            if( gerber->m_FileFunction->IsDrillFile() )
            {
                name.Printf( wxT( "%s (%s,%s,%s,%s)" ),
                             filename.GetData(),
                             gerber->m_FileFunction->GetFileType(),
                             gerber->m_FileFunction->GetDrillLayerPair(),
                             gerber->m_FileFunction->GetLPType(),
                             gerber->m_FileFunction->GetRouteType() );
            }
            else
            {
                name.Printf( wxT( "%s (%s, %s)" ),
                             filename.GetData(),
                             gerber->m_FileFunction->GetFileType(),
                             gerber->m_FileFunction->GetBrdLayerId() );
            }
        }
        else
        {
            name = filename;
        }

        if( aNameOnly )
            return name;

        wxString fullname;

        fullname.Printf( wxT( "%d " ), aIdx + 1 );
        fullname << name;
        return fullname;
    }
    else
    {
        name.Printf( _( "Graphic layer %d" ), aIdx + 1 );
    }

    return name;
}


struct GERBER_ORDER
{
    std::string       m_FilenameMask;
    GERBER_ORDER_ENUM m_Order;
};


// clang-format off
static struct GERBER_ORDER gerberFileExtensionOrder[] =
{
       { ".GM1",        GERBER_ORDER_ENUM::GERBER_BOARD_OUTLINE },
       { ".GM3",        GERBER_ORDER_ENUM::GERBER_BOARD_OUTLINE },
       { ".GBR",        GERBER_ORDER_ENUM::GERBER_BOARD_OUTLINE },
       { ".DIM",        GERBER_ORDER_ENUM::GERBER_BOARD_OUTLINE },
       { ".MIL",        GERBER_ORDER_ENUM::GERBER_BOARD_OUTLINE },
       { ".GML",        GERBER_ORDER_ENUM::GERBER_BOARD_OUTLINE },
       { "EDGE.CUTS",   GERBER_ORDER_ENUM::GERBER_BOARD_OUTLINE },
       { ".FAB",        GERBER_ORDER_ENUM::GERBER_BOARD_OUTLINE },

       { ".GKO",        GERBER_ORDER_ENUM::GERBER_KEEP_OUT },

       { ".GM?",        GERBER_ORDER_ENUM::GERBER_MECHANICAL },
       { ".GM??",       GERBER_ORDER_ENUM::GERBER_MECHANICAL },

       { ".TXT",        GERBER_ORDER_ENUM::GERBER_DRILL },
       { ".XLN",        GERBER_ORDER_ENUM::GERBER_DRILL },
       { ".TAP",        GERBER_ORDER_ENUM::GERBER_DRILL },
       { ".DRD",        GERBER_ORDER_ENUM::GERBER_DRILL },
       { ".DRL",        GERBER_ORDER_ENUM::GERBER_DRILL },
       { ".NC",         GERBER_ORDER_ENUM::GERBER_DRILL },
       { ".XNC",        GERBER_ORDER_ENUM::GERBER_DRILL },

       { ".GTP",        GERBER_ORDER_ENUM::GERBER_TOP_PASTE },
       { ".CRC",        GERBER_ORDER_ENUM::GERBER_TOP_PASTE },
       { ".TSP",        GERBER_ORDER_ENUM::GERBER_TOP_PASTE },
       { "F.PASTE",     GERBER_ORDER_ENUM::GERBER_TOP_PASTE },
       { ".SPT",        GERBER_ORDER_ENUM::GERBER_TOP_PASTE },
       { "PT.PHO",      GERBER_ORDER_ENUM::GERBER_TOP_PASTE },

       { ".GTO",        GERBER_ORDER_ENUM::GERBER_TOP_SILK_SCREEN },
       { ".PLC",        GERBER_ORDER_ENUM::GERBER_TOP_SILK_SCREEN },
       { ".TSK",        GERBER_ORDER_ENUM::GERBER_TOP_SILK_SCREEN },
       { "F.SILKS",     GERBER_ORDER_ENUM::GERBER_TOP_SILK_SCREEN },
       { ".SST",        GERBER_ORDER_ENUM::GERBER_TOP_SILK_SCREEN },
       { "ST.PHO",      GERBER_ORDER_ENUM::GERBER_TOP_SILK_SCREEN },

       { ".GTS",        GERBER_ORDER_ENUM::GERBER_TOP_SOLDER_MASK },
       { ".STC",        GERBER_ORDER_ENUM::GERBER_TOP_SOLDER_MASK },
       { ".TSM",        GERBER_ORDER_ENUM::GERBER_TOP_SOLDER_MASK },
       { "F.MASK",      GERBER_ORDER_ENUM::GERBER_TOP_SOLDER_MASK },
       { ".SMT",        GERBER_ORDER_ENUM::GERBER_TOP_SOLDER_MASK },
       { "MT.PHO",      GERBER_ORDER_ENUM::GERBER_TOP_SOLDER_MASK },

       { ".GTL",        GERBER_ORDER_ENUM::GERBER_TOP_COPPER },
       { ".CMP",        GERBER_ORDER_ENUM::GERBER_TOP_COPPER },
       { ".TOP",        GERBER_ORDER_ENUM::GERBER_TOP_COPPER },
       { "F.CU",        GERBER_ORDER_ENUM::GERBER_TOP_COPPER },
       { "L1.PHO",      GERBER_ORDER_ENUM::GERBER_TOP_COPPER },
       { ".PHD",        GERBER_ORDER_ENUM::GERBER_TOP_COPPER },
       { ".ART",        GERBER_ORDER_ENUM::GERBER_TOP_COPPER },

       { ".GBL",        GERBER_ORDER_ENUM::GERBER_BOTTOM_COPPER },
       { ".SOL",        GERBER_ORDER_ENUM::GERBER_BOTTOM_COPPER },
       { ".BOT",        GERBER_ORDER_ENUM::GERBER_BOTTOM_COPPER },
       { "B.CU",        GERBER_ORDER_ENUM::GERBER_BOTTOM_COPPER },
       { ".BOT",        GERBER_ORDER_ENUM::GERBER_BOTTOM_COPPER },

       { ".GBS",        GERBER_ORDER_ENUM::GERBER_BOTTOM_SOLDER_MASK },
       { ".STS",        GERBER_ORDER_ENUM::GERBER_BOTTOM_SOLDER_MASK },
       { ".BSM",        GERBER_ORDER_ENUM::GERBER_BOTTOM_SOLDER_MASK },
       { "B.MASK",      GERBER_ORDER_ENUM::GERBER_BOTTOM_SOLDER_MASK },
       { ".SMB",        GERBER_ORDER_ENUM::GERBER_BOTTOM_SOLDER_MASK },
       { "MB.PHO",      GERBER_ORDER_ENUM::GERBER_BOTTOM_SOLDER_MASK },

       { ".GBO",        GERBER_ORDER_ENUM::GERBER_BOTTOM_SILK_SCREEN },
       { ".PLS",        GERBER_ORDER_ENUM::GERBER_BOTTOM_SILK_SCREEN },
       { ".BSK",        GERBER_ORDER_ENUM::GERBER_BOTTOM_SILK_SCREEN },
       { "B.SILK",      GERBER_ORDER_ENUM::GERBER_BOTTOM_SILK_SCREEN },
       { ".SSB",        GERBER_ORDER_ENUM::GERBER_BOTTOM_SILK_SCREEN },
       { "SB.PHO",      GERBER_ORDER_ENUM::GERBER_BOTTOM_SILK_SCREEN },

       { ".GBP",        GERBER_ORDER_ENUM::GERBER_BOTTOM_PASTE },
       { ".CRS",        GERBER_ORDER_ENUM::GERBER_BOTTOM_PASTE },
       { ".BSP",        GERBER_ORDER_ENUM::GERBER_BOTTOM_PASTE },
       { "B.PASTE",     GERBER_ORDER_ENUM::GERBER_BOTTOM_PASTE },
       { ".SMB",        GERBER_ORDER_ENUM::GERBER_BOTTOM_PASTE },
       { "MB.PHO",      GERBER_ORDER_ENUM::GERBER_BOTTOM_PASTE },

       // EAGLE CAD file to explicitly ignore that can match some other
       // layers otherwise
       { ".GPI",        GERBER_ORDER_ENUM::GERBER_LAYER_UNKNOWN },

       // Inner copper layers need to come last so the wildcard
       // number matching doesn't pick up other specific layer names.
       { ".GI?",        GERBER_ORDER_ENUM::GERBER_INNER },
       { ".GI??",       GERBER_ORDER_ENUM::GERBER_INNER },
       { ".G?",         GERBER_ORDER_ENUM::GERBER_INNER },
       { ".G??",        GERBER_ORDER_ENUM::GERBER_INNER },
       { ".G?L",        GERBER_ORDER_ENUM::GERBER_INNER },
       { ".G??L",       GERBER_ORDER_ENUM::GERBER_INNER },
};
// clang-format on


void GERBER_FILE_IMAGE_LIST::GetGerberLayerFromFilename( const wxString&         filename,
                                                         enum GERBER_ORDER_ENUM& order,
                                                         wxString&               matchedExtension )
{
    order = GERBER_ORDER_ENUM::GERBER_LAYER_UNKNOWN;
    matchedExtension = "";

    for( struct GERBER_ORDER o : gerberFileExtensionOrder )
    {
        wxString ext = filename.Right( o.m_FilenameMask.length() ).Upper();

        if( ext.Matches( o.m_FilenameMask ) )
        {
            order = o.m_Order;
            matchedExtension = ext;
            return;
        }
    }
}


static bool sortFileExtension( const GERBER_FILE_IMAGE* const& ref,
                               const GERBER_FILE_IMAGE* const& test )
{
    // Do not change order: no criteria to sort items
    if( !ref && !test )
        return false;

    // Not used: ref ordered after
    if( !ref || !ref->m_InUse )
        return false;

    // Not used: ref ordered before
    if( !test || !test->m_InUse )
        return true;

    enum GERBER_ORDER_ENUM ref_layer;
    enum GERBER_ORDER_ENUM test_layer;
    wxString               ref_extension;
    wxString               test_extension;

    GERBER_FILE_IMAGE_LIST::GetGerberLayerFromFilename( ref->m_FileName, ref_layer,
                                                               ref_extension );
    GERBER_FILE_IMAGE_LIST::GetGerberLayerFromFilename( test->m_FileName, test_layer,
                                                               test_extension );

    // Inner layers have a numeric code that we can compare against
    if( ref_layer == GERBER_ORDER_ENUM::GERBER_INNER
        && test_layer == GERBER_ORDER_ENUM::GERBER_INNER )
    {
        unsigned long ref_layer_number = 0;
        unsigned long test_layer_number = 0;

        // Strip extensions down to only the numbers in it. Later conversion to int will
        // automatically skip the spaces
        for( wxString::iterator it = ref_extension.begin(); it != ref_extension.end(); ++it )
        {
            if( !isdigit( *it ) )
                *it = ' ';
        }

        for( wxString::iterator it = test_extension.begin(); it != test_extension.end(); ++it )
        {
            if( !isdigit( *it ) )
                *it = ' ';
        }

        ref_extension.ToULong( &ref_layer_number );
        test_extension.ToULong( &test_layer_number );

        return ref_layer_number < test_layer_number;
    }

    return (int) ref_layer < (int) test_layer;
}


// Helper function, for std::sort.
// Sort loaded images by Z order priority, if they have the X2 FileFormat info
// returns true if the first argument (ref) is ordered before the second (test).
static bool sortZorder( const GERBER_FILE_IMAGE* const& ref, const GERBER_FILE_IMAGE* const& test )
{
    if( !ref && !test )
        return false;        // do not change order: no criteria to sort items

    if( !ref || !ref->m_InUse )
        return false;       // Not used: ref ordered after

    if( !test || !test->m_InUse )
        return true;        // Not used: ref ordered before

    if( !ref->m_FileFunction && !test->m_FileFunction )
        return false;        // do not change order: no criteria to sort items

    if( !ref->m_FileFunction )
        return false;

    if( !test->m_FileFunction )
        return true;

    if( ref->m_FileFunction->GetZOrder() != test->m_FileFunction->GetZOrder() )
        return ref->m_FileFunction->GetZOrder() > test->m_FileFunction->GetZOrder();

    return ref->m_FileFunction->GetZSubOrder() > test->m_FileFunction->GetZSubOrder();
}


std::unordered_map<int, int> GERBER_FILE_IMAGE_LIST::GetLayerRemap()
{
    // The image order has changed.
    // Graphic layer numbering must be updated to match the widgets layer order

    // Store the old/new graphic layer info:
    std::unordered_map<int, int> tab_lyr;

    for( unsigned layer = 0; layer < m_GERBER_List.size(); ++layer )
    {
        GERBER_FILE_IMAGE* gerber = m_GERBER_List[layer];

        if( !gerber )
            continue;

        tab_lyr[gerber->m_GraphicLayer] = layer;
        gerber->m_GraphicLayer = layer ;
    }

    return tab_lyr;
}


std::unordered_map<int, int>
GERBER_FILE_IMAGE_LIST::SortImagesByFunction( LayerSortFunction sortFunction )
{
    std::sort( m_GERBER_List.begin(), m_GERBER_List.end(), sortFunction );
    return GetLayerRemap();
}


std::unordered_map<int, int> GERBER_FILE_IMAGE_LIST::SortImagesByFileExtension()
{
    return SortImagesByFunction( sortFileExtension );
}


std::unordered_map<int, int> GERBER_FILE_IMAGE_LIST::SortImagesByZOrder()
{
    return SortImagesByFunction( sortZorder );
}


// Display group order in the Layer Manager / canvas Z-order.  Lower number
// means "higher in the stack" (closer to the viewer): drill comes first so
// that drill holes show on top of every metal layer, the assembled top side
// follows, then inner copper, then the bottom side; anything without an
// assignment falls into a single tail "Unmapped" bucket.
static int layerRoleDisplayGroupOrder( GERBER_LAYER_GROUP aGroup )
{
    switch( aGroup )
    {
    case GERBER_LAYER_GROUP::Drill: return 0;
    case GERBER_LAYER_GROUP::Top: return 1;
    case GERBER_LAYER_GROUP::Inner: return 2;
    case GERBER_LAYER_GROUP::Bottom: return 3;
    case GERBER_LAYER_GROUP::Unmapped: return 4;
    default: return 5;
    }
}


/// Resolve the role currently bound to @p aImg using the persisted assignments
/// table.  Returns Unassigned when no override exists, when settings are
/// unavailable, or when the image has no usable filename.
static GERBVIEW_LAYER_ROLE roleForImage( const GERBVIEW_SETTINGS* aCfg, const GERBER_FILE_IMAGE* aImg )
{
    if( !aCfg || !aImg )
        return GERBVIEW_LAYER_ROLE::Unassigned;

    wxString base = wxFileName( aImg->m_FileName ).GetFullName();

#ifdef __WINDOWS__
    base.MakeLower();
#endif

    auto it = aCfg->m_LayerRoles.assignments.find( base );

    if( it == aCfg->m_LayerRoles.assignments.end() )
        return GERBVIEW_LAYER_ROLE::Unassigned;

    return it->second;
}


std::unordered_map<int, int> GERBER_FILE_IMAGE_LIST::SortImagesByLayerRoles( const GERBVIEW_SETTINGS* aCfg )
{
    std::stable_sort( m_GERBER_List.begin(), m_GERBER_List.end(),
                      [aCfg]( const GERBER_FILE_IMAGE* const& ref, const GERBER_FILE_IMAGE* const& test ) -> bool
                      {
                          // Empty / unused slots collapse to the end of the vector,
                          // mirroring the existing sortFileExtension/sortZorder rules.
                          if( !ref && !test )
                              return false;

                          if( !ref || !ref->m_InUse )
                              return false;

                          if( !test || !test->m_InUse )
                              return true;

                          GERBVIEW_LAYER_ROLE refRole = roleForImage( aCfg, ref );
                          GERBVIEW_LAYER_ROLE testRole = roleForImage( aCfg, test );

                          // Use the *effective* group so unmapped drill files (.drl,
                          // .ncd, .xnc, ...) get promoted into the Drill bucket the
                          // same way the Layer Manager bucketizes them.  This is what
                          // keeps the canvas Z-order (drill on top) and the Layer
                          // Manager grouping (drill section first) consistent.
                          GERBER_LAYER_GROUP refGroup = EffectiveGroupForImage( ref, refRole );
                          GERBER_LAYER_GROUP testGroup = EffectiveGroupForImage( test, testRole );

                          int refGo = layerRoleDisplayGroupOrder( refGroup );
                          int testGo = layerRoleDisplayGroupOrder( testGroup );

                          if( refGo != testGo )
                              return refGo < testGo;

                          // Drill bucket holds two distinct populations: files with an
                          // explicit Drill/Contour role assignment (RoleOrderInGroup
                          // values 0..3) and unmapped drill files promoted by extension.
                          // The explicit ones should sit above the extension-detected
                          // tail; that ordering already falls out of the role-order
                          // comparison below as long as we treat unmapped-in-drill as
                          // higher than any real role.
                          if( refGroup == GERBER_LAYER_GROUP::Unmapped )
                          {
                              enum GERBER_ORDER_ENUM refLayer;
                              enum GERBER_ORDER_ENUM testLayer;
                              wxString               refExt;
                              wxString               testExt;

                              GetGerberLayerFromFilename( ref->m_FileName, refLayer, refExt );
                              GetGerberLayerFromFilename( test->m_FileName, testLayer, testExt );

                              if( refLayer != testLayer )
                                  return static_cast<int>( refLayer ) < static_cast<int>( testLayer );

                              return ref->m_FileName.Cmp( test->m_FileName ) < 0;
                          }

                          int refRo = EffectiveRoleOrderInGroup( refGroup, refRole );
                          int testRo = EffectiveRoleOrderInGroup( testGroup, testRole );

                          if( refRo != testRo )
                              return refRo < testRo;

                          // Same role (e.g. two files both flagged TopCopper): keep a
                          // deterministic order based on filename so re-sorts are
                          // stable across runs.
                          return ref->m_FileName.Cmp( test->m_FileName ) < 0;
                      } );

    return GetLayerRemap();
}


std::unordered_map<int, int> GERBER_FILE_IMAGE_LIST::SwapImages( unsigned int layer1,
                                                                 unsigned int layer2 )
{
    if( ( layer1 >= m_GERBER_List.size() ) || ( layer2 >= m_GERBER_List.size() ) )
        return std::unordered_map<int, int>();

    std::swap( m_GERBER_List[layer1], m_GERBER_List[layer2] );
    return GetLayerRemap();
}

std::unordered_map<int, int> GERBER_FILE_IMAGE_LIST::RemoveImage( unsigned int layer )
{
    if( layer >= m_GERBER_List.size() )
        return std::unordered_map<int, int>();

    DeleteImage( layer );
    // Move deleted image to end of list, move all other images up
    std::rotate( m_GERBER_List.begin() + layer, m_GERBER_List.begin() + layer + 1,
                 m_GERBER_List.end() );
    return GetLayerRemap();
}
