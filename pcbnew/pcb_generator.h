/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2023 Alex Shvartzkop <dudesuchamazing@gmail.com>
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef GENERATOR_H_
#define GENERATOR_H_


#include <unordered_set>
#include <string_any_map.h>

#include <lset.h>
#include <pcb_group.h>
#include <geometry/shape_poly_set.h>

class EDIT_POINTS;
class BOARD;
class BOARD_ITEM;
class PCB_BASE_EDIT_FRAME;
class GENERATOR_TOOL;
class STATUS_MIN_MAX_POPUP;
class ACTION_MENU;
class TOOL_INTERACTIVE;


class PCB_GENERATOR : public PCB_GROUP
{
public:

    PCB_GENERATOR( BOARD_ITEM* aParent, PCB_LAYER_ID aLayer );

    virtual ~PCB_GENERATOR();

    /*
     * Clone() this and all descendants
     */
    PCB_GENERATOR* DeepClone() const;

    virtual void EditStart( GENERATOR_TOOL* aTool, BOARD* aBoard, BOARD_COMMIT* aCommit ) = 0;
    virtual bool Update( GENERATOR_TOOL* aTool, BOARD* aBoard, BOARD_COMMIT* aCommit ) = 0;
    virtual void EditFinish( GENERATOR_TOOL* aTool, BOARD* aBoard, BOARD_COMMIT* aCommit ) = 0;
    virtual void EditCancel( GENERATOR_TOOL* aTool, BOARD* aBoard, BOARD_COMMIT* aCommit ) = 0;

    virtual void Remove( GENERATOR_TOOL* aTool, BOARD* aBoard, BOARD_COMMIT* aCommit ) = 0;

    /**
     * @return true if individual children of this generator can be selected, picked, and
     *         right-clicked independently of the parent.  Default: false (the standard
     *         group-promotion behavior — clicks on a child select the parent generator).
     */
    virtual bool ChildrenAreIndividuallySelectable() const { return false; }

    /**
     * Build (or return a cached) context menu for a selection consisting of children of
     * *this* generator.  Implementations should own the returned menu's lifetime.
     *
     * @return a non-owning ACTION_MENU* to display, or nullptr to fall through to the
     *         standard selection menu.
     */
    virtual ACTION_MENU* GetChildContextMenu( TOOL_INTERACTIVE* aTool ) const { return nullptr; }

#define STATUS_ITEMS_ONLY true

    virtual std::vector<EDA_ITEM*> GetPreviewItems( GENERATOR_TOOL* aTool,
                                                    PCB_BASE_EDIT_FRAME* aFrame,
                                                    bool aStatusItemsOnly = false );

    virtual bool MakeEditPoints( EDIT_POINTS& aEditPoints ) const;

    virtual bool UpdateFromEditPoints( EDIT_POINTS& aEditPoints );

    virtual bool UpdateEditPoints( EDIT_POINTS& aEditPoints );

    const BOX2I GetBoundingBox() const override;

    VECTOR2I GetPosition() const override { return m_origin; }
    void SetPosition( const VECTOR2I& aPos ) override { m_origin = aPos; }

    void Move( const VECTOR2I& aMoveVector ) override;

    void Rotate( const VECTOR2I& aRotCentre, const EDA_ANGLE& aAngle ) override;

    void Flip( const VECTOR2I& aCentre, FLIP_DIRECTION aFlipDirection ) override;

    void Mirror( const VECTOR2I& aCentre, FLIP_DIRECTION aMirrorDirection ) override;

    LSET GetLayerSet() const override;

    virtual void SetLayer( PCB_LAYER_ID aLayer ) override;

    virtual wxString GetGeneratorType() const;

    virtual const STRING_ANY_MAP GetProperties() const;

    virtual void SetProperties( const STRING_ANY_MAP& aProps );

    virtual std::vector<std::pair<wxString, wxVariant>> GetRowData();

    virtual void ShowPropertiesDialog( PCB_BASE_EDIT_FRAME* aEditFrame ) {};

    wxString GetItemDescription( UNITS_PROVIDER* aUnitsProvider, bool aFull ) const override;

    virtual wxString GetPluralName() const = 0;
    virtual wxString GetCommitMessage() const = 0;

#if defined(DEBUG)
    void Show( int nestLevel, std::ostream& os ) const override { ShowDummy( os ); }
#endif

    wxString GetClass() const override;

    static inline bool ClassOf( const EDA_ITEM* aItem );

#ifdef GENERATOR_ORDER
    int  GetUpdateOrder() const { return m_updateOrder; }
    void SetUpdateOrder( int aValue ) { m_updateOrder = aValue; }
#endif

protected:
    wxString m_generatorType;

    VECTOR2I m_origin;

#ifdef GENERATOR_ORDER
    int m_updateOrder = 0;
#endif

    friend class GENERATORS_MGR;

    void baseMirror( const VECTOR2I& aCentre, FLIP_DIRECTION aFlipDirection );
};

class PCB_GENERATOR_POLY : public PCB_GENERATOR
{
public:
    PCB_GENERATOR_POLY( BOARD_ITEM* aParent, PCB_LAYER_ID aLayer ) :
            PCB_GENERATOR( aParent, aLayer )
    {
    }

    SHAPE_POLY_SET&       Outline() { return m_outline; }
    const SHAPE_POLY_SET& Outline() const { return m_outline; }

    void SetOutline( SHAPE_POLY_SET& aOutline ) { m_outline = aOutline; }

    void Move( const VECTOR2I& aMoveVector ) override
    {
        m_outline.Move( aMoveVector );
        PCB_GENERATOR::Move( aMoveVector );
    }

    void Rotate( const VECTOR2I& aRotCentre, const EDA_ANGLE& aAngle ) override
    {
        m_outline.Rotate( aAngle, aRotCentre );
        PCB_GENERATOR::Rotate( aRotCentre, aAngle );
    }

    void Flip( const VECTOR2I& aCentre, FLIP_DIRECTION aFlipDirection ) override
    {
        m_outline.Mirror( aCentre, aFlipDirection );
        PCB_GENERATOR::Flip( aCentre, aFlipDirection );
    }

    void Mirror( const VECTOR2I& aCentre, FLIP_DIRECTION aMirrorDirection ) override
    {
        m_outline.Mirror( aCentre, aMirrorDirection );
        PCB_GENERATOR::Mirror( aCentre, aMirrorDirection );
    }

protected:
    SHAPE_POLY_SET m_outline;
};

#endif /* GENERATOR_H_ */
