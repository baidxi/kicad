/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
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

#pragma once

#include <pcb_generator.h>
#include <generators_mgr.h>
#include <i18n_utility.h>
#include <geometry/shape_poly_set.h>
#include <memory>
#include <set>

class ACTION_MENU;
class TOOL_INTERACTIVE;
class PCB_VIA;


enum class PCB_VIA_STITCH_LAYOUT
{
    PLAIN,      ///< Regular row/column grid.
    STAGGERED   ///< Odd rows shifted by half the pitch.
};


class PCB_VIA_STITCH : public PCB_GENERATOR_POLY
{
public:
    static const wxString GENERATOR_TYPE;
    static const wxString DISPLAY_NAME;

    PCB_VIA_STITCH( BOARD_ITEM* aParent = nullptr, PCB_LAYER_ID aLayer = F_Cu );
    PCB_VIA_STITCH( const PCB_VIA_STITCH& aOther );
    PCB_VIA_STITCH( PCB_VIA_STITCH&& aOther ) noexcept;
    PCB_VIA_STITCH& operator=( const PCB_VIA_STITCH& aOther );
    PCB_VIA_STITCH& operator=( PCB_VIA_STITCH&& aOther ) noexcept;
    ~PCB_VIA_STITCH() override;

    wxString GetGeneratorType() const override { return wxS( "via_stitch" ); }

    wxString GetItemDescription( UNITS_PROVIDER* aUnitsProvider, bool aFull ) const override
    {
        return wxString( _( "Via Stitching" ) );
    }

    wxString GetFriendlyName() const override { return wxString( _( "Via Stitching" ) ); }

    wxString GetPluralName() const override { return wxString( _( "Via Stitching" ) ); }

    void ViewDraw( int aLayer, KIGFX::VIEW* aView ) const override final;

    std::vector<int> ViewGetLayers() const override
    {
        return std::vector<int>{ LAYER_VIAS, LAYER_VIA_HOLES, LAYER_ANCHOR, GetLayer() };
    }
    #if 0
    void SetPolyShape( const SHAPE_POLY_SET& aShape )
    {
        m_poly = aShape;

        for( int ii = 0; ii < m_poly.OutlineCount(); ++ii )
        {
            if( m_poly.HoleCount( ii ) )
            {
                m_poly.Fracture();
                break;
            }
        }
    }
#endif

    int GetNetCode() const;

    void        SetNetCode( int aNetCode );

    const BOX2I GetBoundingBox() const override
    {
        return m_outline.BBox();
    }

    bool HitTest( const VECTOR2I& aPosition, int aAccuracy = 0 ) const override
    {
        return m_outline.Collide( aPosition, aAccuracy );
    }

    VECTOR2I GetPosition() const override;

    /**
     * This method is overrided to translate the stitch zone outline only on move.
     * Otherwise the default Move() implementation would move the child vias as well.
     * As the outline moves, the next Update() call will add/remove vias as needed on the stitch grid.
     */
    void Move( const VECTOR2I& aMoveVector ) override
    {
        m_outline.Move( aMoveVector );
        m_origin += aMoveVector;
    }

    void     EditStart( GENERATOR_TOOL*, BOARD*, BOARD_COMMIT* ) override;
    bool     Update( GENERATOR_TOOL*, BOARD*, BOARD_COMMIT* ) override; // generate vias
    void     EditFinish( GENERATOR_TOOL*, BOARD*, BOARD_COMMIT* ) override;
    void     EditCancel( GENERATOR_TOOL*, BOARD*, BOARD_COMMIT* ) override;
    void Remove( GENERATOR_TOOL*, BOARD*, BOARD_COMMIT* ) override;
    wxString GetCommitMessage() const override { return _( "Stitching Update" ); }

    // Properties 
    int  GetViaSize() const;
    void SetViaSize( int aVal );
    int  GetViaDrill() const;
    void SetViaDrill( int aVal );
    int  GetPitch() const { return m_pitch; }
    void SetPitch( int aVal ) { m_pitch = aVal; }
    PCB_VIA_STITCH_LAYOUT GetLayout() const { return m_layout; }
    void          SetLayout( PCB_VIA_STITCH_LAYOUT aVal ) { m_layout = aVal; }

    PCB_VIA*       ViaTemplate() { return m_viaTemplate.get(); }
    const PCB_VIA* ViaTemplate() const { return m_viaTemplate.get(); }

    /** Remove a board position from the exclusion set so the next Update() places a via there. */
    void ClearExclusion( const VECTOR2I& aPos );

    /** Add a board position to the exclusion set so the next Update() skips it.  The position
     *  is converted to a grid cell index so the exclusion follows the grid as the offset or
     *  outline changes. */
    void ExcludePosition( const VECTOR2I& aPos );

    /** Drop all manual exclusions. */
    void ClearAllExclusions();

    const STRING_ANY_MAP GetProperties() const override;
    void SetProperties( const STRING_ANY_MAP& aProps ) override;

    EDA_ITEM* Clone() const override { return new PCB_VIA_STITCH( *this ); }

    bool ChildrenAreIndividuallySelectable() const override { return true; }

    ACTION_MENU* GetChildContextMenu( TOOL_INTERACTIVE* aTool ) const override;

protected:
    void swapData( BOARD_ITEM* aImage ) override
    {
        wxASSERT( aImage->Type() == PCB_GENERATOR_T );
        std::swap( *this, *static_cast<PCB_VIA_STITCH*>( aImage ) );
    }

private:
    int defaultPitch( BOARD* aBoard ) const;      // grid pitch
    int defaultViaSize( BOARD* aBoard ) const;    // via diameter
    int defaultViaDrill( BOARD* aBoard ) const;   // drill diameter

    /** Convert an absolute board position to a (col, row) grid cell index. */
    VECTOR2I cellForPosition( const VECTOR2I& aPos ) const;

    /** Convert a (col, row) grid cell index to an absolute board position, including
     *  any stagger shift on odd rows. 
     */
    VECTOR2I positionForCell( const VECTOR2I& aCell, int aPitch ) const;

    /** Walk the placement grid, run DRC clearance tests, and return the set of (col, row)
     *  cells where a via fits cleanly.  Does not consult exclusions — that's a separate
     *  user-driven concern applied at consumption time. 
     */
    std::set<VECTOR2I> buildPlacementCells( BOARD* aBoard ) const;

    /**
     * Returns true if @a aVia violates the required copper clearance against any existing board
     * item (ignoring same-net connected items).  The via must NOT yet have been added to the
     * board so that the R-tree query only sees pre-existing items.
     *
     * @param aBoard Board to test against
     * @param aDRCEngine Used to retrieve DRC clearance constraints for the via vs. other items.
     * @param aVia The via to test for clearance violations
     */
    bool collidesWithBoard( BOARD* aBoard, class DRC_ENGINE* aDRCEngine, PCB_VIA* aVia ) const;

private:
 //   SHAPE_POLY_SET m_poly; // Stores the S_POLYGON shape

    // Stored properties (iu units)
    int           m_pitch = 0;                            // center-to-center spacing
    PCB_VIA_STITCH_LAYOUT m_layout = PCB_VIA_STITCH_LAYOUT::PLAIN;

    std::unique_ptr<PCB_VIA> m_viaTemplate;

    wxString m_lastNetName;
    int      m_netCode = 0;

    // Grid cells (col, row) the user has explicitly excluded.  Tracking by cell index instead
    // of absolute position lets exclusions follow the grid as m_originOffset or the outline
    // change.  Persisted so regeneration skips them.
    std::set<VECTOR2I> m_excludedCells;

    // Offset of the placement grid relative to the global (0,0) anchor, in [0, pitch) on each
    // axis.  Updated when the user drags an individual stitching via — that via's new
    // position implies a new offset and the entire grid re-aligns to it.
    VECTOR2I m_originOffset{ 0, 0 };

    // Lazily-built right-click menu shown when the selection is exclusively this generator's
    // child vias.  mutable so it can be created from a const accessor.
    mutable std::unique_ptr<ACTION_MENU> m_childContextMenu;
};