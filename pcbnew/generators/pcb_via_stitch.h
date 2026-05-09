#ifndef VIA_STITCH
#define VIA_STITCH

#include <pcb_generator.h>
#include <generators_mgr.h>
#include <i18n_utility.h>
#include <geometry/shape_poly_set.h>
#include <memory>
#include <set>

class ACTION_MENU;
class TOOL_INTERACTIVE;

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
    int  GetViaSize() const { return m_viaSize; }
    void SetViaSize( int aVal ) { m_viaSize = aVal; }
    int  GetViaDrill() const { return m_viaDrill; }
    void SetViaDrill( int aVal ) { m_viaDrill = aVal; }
    int  GetPitch() const { return m_pitch; }
    void SetPitch( int aVal ) { m_pitch = aVal; }
    bool GetStagger() const { return m_stagger; }
    void SetStagger( bool aVal ) { m_stagger = aVal; }

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
     *  any stagger shift on odd rows. */
    VECTOR2I positionForCell( const VECTOR2I& aCell, int aPitch ) const;

    /** Walk the placement grid, run DRC clearance tests, and return the set of (col, row)
     *  cells where a via fits cleanly.  Does not consult exclusions — that's a separate
     *  user-driven concern applied at consumption time. */
    std::set<VECTOR2I> buildPlacementCells( BOARD* aBoard ) const;

private:
 //   SHAPE_POLY_SET m_poly; // Stores the S_POLYGON shape

    // Stored properties (iu units)
    int  m_viaSize = 0;
    int  m_viaDrill = 0;
    int  m_pitch = 0;    // center-to-center spacing
    bool m_stagger = false; // offset alternating rows by half-pitch

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

#endif