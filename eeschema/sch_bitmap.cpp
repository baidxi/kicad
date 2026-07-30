/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2011 jean-pierre.charras
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

/**
 * @file sch_bitmap.cpp
 */
#include "sch_bitmap.h"

#include <bitmap_base.h>
#include <bitmaps.h>
#include <base_units.h>
#include <common.h>
#include <core/mirror.h>
#include <eda_draw_frame.h>
#include <geometry/geometry_utils.h>
#include <plotters/plotter.h>
#include <sch_draw_panel.h>
#include <settings/color_settings.h>
#include <trigo.h>
#include <api/api_utils.h>
#include <api/schematic/schematic_types.pb.h>

#include <cstring>

#include <properties/property.h>
#include <properties/property_mgr.h>


static int compareInts( int aLeft, int aRight )
{
    if( aLeft == aRight )
        return 0;

    return aLeft < aRight ? -1 : 1;
}


static int compareBools( bool aLeft, bool aRight )
{
    return compareInts( static_cast<int>( aLeft ), static_cast<int>( aRight ) );
}


static int compareByteData( const unsigned char* aLeft, const unsigned char* aRight, size_t aSize )
{
    if( !aLeft || !aRight )
    {
        if( aLeft == aRight )
            return 0;

        return aLeft ? 1 : -1;
    }

    int retv = std::memcmp( aLeft, aRight, aSize );

    if( retv )
        return retv < 0 ? -1 : 1;

    return 0;
}


static int compareDecodedImages( const BITMAP_BASE& aLeft, const BITMAP_BASE& aRight )
{
    const wxImage* leftImage = aLeft.GetImageData();
    const wxImage* rightImage = aRight.GetImageData();

    if( !leftImage || !rightImage )
    {
        if( leftImage == rightImage )
            return 0;

        return leftImage ? 1 : -1;
    }

    if( int retv = compareInts( leftImage->GetWidth(), rightImage->GetWidth() ) )
        return retv;

    if( int retv = compareInts( leftImage->GetHeight(), rightImage->GetHeight() ) )
        return retv;

    const size_t pixelCount =
            static_cast<size_t>( leftImage->GetWidth() ) * static_cast<size_t>( leftImage->GetHeight() );

    if( int retv = compareByteData( leftImage->GetData(), rightImage->GetData(), pixelCount * 3 ) )
        return retv;

    if( int retv = compareBools( leftImage->HasAlpha(), rightImage->HasAlpha() ) )
        return retv;

    if( leftImage->HasAlpha() )
    {
        if( int retv = compareByteData( leftImage->GetAlpha(), rightImage->GetAlpha(), pixelCount ) )
            return retv;
    }

    if( int retv = compareBools( leftImage->HasMask(), rightImage->HasMask() ) )
        return retv;

    if( leftImage->HasMask() )
    {
        if( int retv = compareInts( leftImage->GetMaskRed(), rightImage->GetMaskRed() ) )
            return retv;

        if( int retv = compareInts( leftImage->GetMaskGreen(), rightImage->GetMaskGreen() ) )
            return retv;

        if( int retv = compareInts( leftImage->GetMaskBlue(), rightImage->GetMaskBlue() ) )
            return retv;
    }

    return 0;
}


SCH_BITMAP::SCH_BITMAP( const VECTOR2I& pos ) :
    SCH_ITEM( nullptr, SCH_BITMAP_T ),
    m_referenceImage( schIUScale)
{
    m_referenceImage.SetPosition( pos );
    m_layer = LAYER_NOTES;              // used only to draw/plot a rectangle,
                                        // when a bitmap cannot be drawn or plotted
}


SCH_BITMAP::SCH_BITMAP( const SCH_BITMAP& aSchBitmap ) :
    SCH_ITEM( aSchBitmap ),
    m_referenceImage( aSchBitmap.m_referenceImage )
{
}


SCH_BITMAP& SCH_BITMAP::operator=( const SCH_ITEM& aItem )
{
    wxCHECK_MSG( Type() == aItem.Type(), *this,
                 wxT( "Cannot assign object type " ) + aItem.GetClass() + wxT( " to type " ) +
                 GetClass() );

    if( &aItem != this )
    {
        SCH_ITEM::operator=( aItem );

        const SCH_BITMAP& bitmap = static_cast<const SCH_BITMAP&>( aItem );
        m_referenceImage = bitmap.m_referenceImage;
    }

    return *this;
}


EDA_ITEM* SCH_BITMAP::Clone() const
{
    return new SCH_BITMAP( *this );
}


void SCH_BITMAP::swapData( SCH_ITEM* aItem )
{
    wxCHECK_RET( aItem->Type() == SCH_BITMAP_T,
                 wxString::Format( wxT( "SCH_BITMAP object cannot swap data with %s object." ),
                                   aItem->GetClass() ) );

    SCH_BITMAP* item = (SCH_BITMAP*) aItem;
    m_referenceImage.SwapData( item->m_referenceImage );
}


const BOX2I SCH_BITMAP::GetBoundingBox() const
{
    return m_referenceImage.GetBoundingBox();
}


VECTOR2I SCH_BITMAP::GetPosition() const
{
    return m_referenceImage.GetPosition();
}


void SCH_BITMAP::SetPosition( const VECTOR2I& aPosition )
{
    m_referenceImage.SetPosition( aPosition );
}


void SCH_BITMAP::Move( const VECTOR2I& aMoveVector )
{
    SetPosition( GetPosition() + aMoveVector );
}


void SCH_BITMAP::Serialize( google::protobuf::Any& aContainer ) const
{
    using namespace kiapi::common;

    kiapi::schematic::types::SchematicImage image;

    image.mutable_id()->set_value( m_Uuid.AsStdString() );
    PackVector2( *image.mutable_position(), m_referenceImage.GetPosition(), schIUScale );
    PackVector2( *image.mutable_transform_origin_offset(), m_referenceImage.GetTransformOriginOffset(), schIUScale );

    image.mutable_image_scale()->set_value( m_referenceImage.GetImageScale() );
    image.set_locked( IsLocked() ? types::LockedState::LS_LOCKED : types::LockedState::LS_UNLOCKED );

    m_referenceImage.PackToBytes( *image.mutable_image_data() );

    aContainer.PackFrom( image );
}


bool SCH_BITMAP::Deserialize( const google::protobuf::Any& aContainer )
{
    using namespace kiapi::common;

    kiapi::schematic::types::SchematicImage image;

    if( !aContainer.UnpackTo( &image ) )
        return false;

    const_cast<KIID&>( m_Uuid ) = KIID( image.id().value() );

    if( !image.image_data().empty() )
    {
        if( !m_referenceImage.UnpackFromBytes( image.image_data() ) )
            return false;
    }

    if( image.has_image_scale() )
        m_referenceImage.SetImageScale( image.image_scale().value() );

    SetPosition( UnpackVector2( image.position(), schIUScale ) );
    m_referenceImage.SetTransformOriginOffset( UnpackVector2( image.transform_origin_offset(), schIUScale ) );

    SetLocked( image.locked() == types::LockedState::LS_LOCKED );
    return true;
}


void SCH_BITMAP::MirrorVertically( int aCenter )
{
    m_referenceImage.Flip( VECTOR2I( 0, aCenter ), FLIP_DIRECTION::TOP_BOTTOM );
}


void SCH_BITMAP::MirrorHorizontally( int aCenter )
{
    m_referenceImage.Flip( VECTOR2I( aCenter, 0 ), FLIP_DIRECTION::LEFT_RIGHT );
}


void SCH_BITMAP::Rotate( const VECTOR2I& aCenter, bool aRotateCCW )
{
    m_referenceImage.Rotate( aCenter, aRotateCCW ? ANGLE_90 : ANGLE_270 );
}


#if defined(DEBUG)
void SCH_BITMAP::Show( int nestLevel, std::ostream& os ) const
{
    // XML output:
    wxString s = GetClass();

    NestedSpace( nestLevel, os ) << '<' << s.Lower().mb_str() << GetPosition() << "/>\n";
}
#endif


bool SCH_BITMAP::HitTest( const VECTOR2I& aPosition, int aAccuracy ) const
{
    return KIGEOM::BoxHitTest( aPosition, GetBoundingBox(), aAccuracy );
}


bool SCH_BITMAP::HitTest( const BOX2I& aRect, bool aContained, int aAccuracy ) const
{
    return KIGEOM::BoxHitTest( aRect, GetBoundingBox(), aContained, aAccuracy );
}


bool SCH_BITMAP::HitTest( const SHAPE_LINE_CHAIN& aPoly, bool aContained ) const
{
    return KIGEOM::BoxHitTest( aPoly, GetBoundingBox(), aContained );
}



void SCH_BITMAP::Plot( PLOTTER* aPlotter, bool aBackground, const SCH_PLOT_OPTS& aPlotOpts,
                       int aUnit, int aBodyStyle, const VECTOR2I& aOffset, bool aDimmed )
{
    if( aBackground )
    {
        SCH_RENDER_SETTINGS* renderSettings = getRenderSettings( aPlotter );
        RENDER_SETTINGS*     cfg = aPlotter->RenderSettings();

        // Symbol-contained bitmaps are stored in symbol-local coordinates.
        // Apply transform and offset so plot/print outputs match the symbol instance placement.
        const TRANSFORM& transform = renderSettings->m_Transform;
        VECTOR2I         pos = transform.TransformCoordinate( GetPosition() ) + aOffset;

        // A raster image cannot be reoriented by a coordinate matrix, so when the symbol
        // instance is rotated/mirrored the transform is reproduced on a copy of the image.
        // Match the canvas path's OrientAndMirrorSymbolItems() sequence: rotate first, then
        // mirror using the canonical orientation returned for the symbol transform matrix.
        // Some mirror+rotation combinations have equivalent matrices, so this mapping follows
        // SCH_SYMBOL::GetOrientation() ordering rather than factoring every reflection through
        // the same mirror axis.
        if( transform != TRANSFORM() )
        {
            BITMAP_BASE    oriented( m_referenceImage.GetImage() );
            int            rotations = 0;
            bool           mirror = false;
            FLIP_DIRECTION mirrorDirection = FLIP_DIRECTION::LEFT_RIGHT;

            if( transform == TRANSFORM( 0, 1, -1, 0 ) )
                rotations = 1;
            else if( transform == TRANSFORM( -1, 0, 0, -1 ) )
                rotations = 2;
            else if( transform == TRANSFORM( 0, -1, 1, 0 ) )
                rotations = 3;
            else if( transform == TRANSFORM( 1, 0, 0, -1 ) )
            {
                mirror = true;
                mirrorDirection = FLIP_DIRECTION::TOP_BOTTOM;
            }
            else if( transform == TRANSFORM( 0, -1, -1, 0 ) )
            {
                rotations = 3;
                mirror = true;
                mirrorDirection = FLIP_DIRECTION::TOP_BOTTOM;
            }
            else if( transform == TRANSFORM( 0, 1, 1, 0 ) )
            {
                rotations = 1;
                mirror = true;
                mirrorDirection = FLIP_DIRECTION::TOP_BOTTOM;
            }
            else if( transform == TRANSFORM( -1, 0, 0, 1 ) )
            {
                mirror = true;
                mirrorDirection = FLIP_DIRECTION::LEFT_RIGHT;
            }
            else
            {
                wxFAIL_MSG( wxString::Format( wxT( "Unhandled symbol bitmap transform [%d %d; %d %d]" ), transform.x1,
                                              transform.y1, transform.x2, transform.y2 ) );
            }

            for( int ii = 0; ii < rotations; ++ii )
                oriented.Rotate( true );

            if( mirror )
                oriented.Mirror( mirrorDirection );

            oriented.PlotImage( aPlotter, pos, cfg->GetLayerColor( GetLayer() ), cfg->GetDefaultPenWidth() );
        }
        else
        {
            m_referenceImage.GetImage().PlotImage( aPlotter, pos, cfg->GetLayerColor( GetLayer() ),
                                                   cfg->GetDefaultPenWidth() );
        }
    }
}


BITMAPS SCH_BITMAP::GetMenuImage() const
{
    return BITMAPS::image;
}


void SCH_BITMAP::GetMsgPanelInfo( EDA_DRAW_FRAME* aFrame, std::vector<MSG_PANEL_ITEM>& aList )
{
    aList.emplace_back( _( "Bitmap" ), wxEmptyString );

    aList.emplace_back( _( "PPI" ),
                        wxString::Format( wxT( "%d " ), m_referenceImage.GetImage().GetPPI() ) );
    aList.emplace_back( _( "Scale" ),
                        wxString::Format( wxT( "%f " ), m_referenceImage.GetImageScale() ) );

    aList.emplace_back( _( "Width" ),
                        aFrame->MessageTextFromValue( m_referenceImage.GetSize().x ) );
    aList.emplace_back( _( "Height" ),
                        aFrame->MessageTextFromValue( m_referenceImage.GetSize().y ) );
}


std::vector<int> SCH_BITMAP::ViewGetLayers() const
{
    return { LAYER_DRAW_BITMAPS, LAYER_SELECTION_SHADOWS };
}


bool SCH_BITMAP::operator==( const SCH_ITEM& aItem ) const
{
    if( Type() != aItem.Type() )
        return false;

    const SCH_BITMAP& bitmap = static_cast<const SCH_BITMAP&>( aItem );
    return m_referenceImage == bitmap.m_referenceImage;
}


int SCH_BITMAP::compare( const SCH_ITEM& aOther, int aCompareFlags ) const
{
    // No same-type assert here: LIB_SYMBOL::Compare sorts shapes and bitmaps in a single
    // set, so cross-type comparisons are legitimate and resolved by the type check below.
    int retv = SCH_ITEM::compare( aOther, aCompareFlags );

    if( retv )
        return retv;

    const SCH_BITMAP& tmp = static_cast<const SCH_BITMAP&>( aOther );

    if( GetImageScale() != tmp.GetImageScale() )
        return GetImageScale() > tmp.GetImageScale() ? 1 : -1;

    if( int tmp2 = LexicographicalCompare( m_referenceImage.GetTransformOriginOffset(),
                                           tmp.m_referenceImage.GetTransformOriginOffset() ) )
    {
        return tmp2;
    }

    VECTOR2I size = m_referenceImage.GetSize();
    VECTOR2I otherSize = tmp.m_referenceImage.GetSize();

    if( int tmp2 = LexicographicalCompare( size, otherSize ) )
        return tmp2;

    if( int tmp2 = compareDecodedImages( m_referenceImage.GetImage(), tmp.m_referenceImage.GetImage() ) )
        return tmp2;

    return 0;
}


double SCH_BITMAP::Similarity( const SCH_ITEM& aItem ) const
{
    if( Type() != aItem.Type() )
        return 0.0;

    if( m_Uuid == aItem.m_Uuid )
        return 1.0;

    const SCH_BITMAP& bitmap = static_cast<const SCH_BITMAP&>( aItem );
    return m_referenceImage.Similarity( bitmap.m_referenceImage );
}


int SCH_BITMAP::GetTransformOriginOffsetX() const
{
    return m_referenceImage.GetTransformOriginOffset().x;
}


void SCH_BITMAP::SetTransformOriginOffsetX( int aX )
{
    VECTOR2I offset = m_referenceImage.GetTransformOriginOffset();
    offset.x = aX;
    m_referenceImage.SetTransformOriginOffset( offset );
}


int SCH_BITMAP::GetTransformOriginOffsetY() const
{
    return m_referenceImage.GetTransformOriginOffset().y;
}


void SCH_BITMAP::SetTransformOriginOffsetY( int aY )
{
    VECTOR2I offset = m_referenceImage.GetTransformOriginOffset();
    offset.y = aY;
    m_referenceImage.SetTransformOriginOffset( offset );
}


double SCH_BITMAP::GetImageScale() const
{
    return m_referenceImage.GetImageScale();
}


void SCH_BITMAP::SetImageScale( double aScale )
{
    m_referenceImage.SetImageScale( aScale );
}


int SCH_BITMAP::GetWidth() const
{
    return m_referenceImage.GetImage().GetSize().x;
}


void SCH_BITMAP::SetWidth( int aWidth )
{
    m_referenceImage.SetWidth( aWidth );
}


int SCH_BITMAP::GetHeight() const
{
    return m_referenceImage.GetImage().GetSize().y;
}


void SCH_BITMAP::SetHeight( int aHeight )
{
    m_referenceImage.SetHeight( aHeight );
}


static struct SCH_BITMAP_DESC
{
    SCH_BITMAP_DESC()
    {
        PROPERTY_MANAGER& propMgr = PROPERTY_MANAGER::Instance();
        REGISTER_TYPE( SCH_BITMAP );
        propMgr.InheritsAfter( TYPE_HASH( SCH_BITMAP ), TYPE_HASH( SCH_ITEM ) );

        propMgr.AddProperty( new PROPERTY<SCH_BITMAP, int>( _HKI( "Position X" ),
                                     &SCH_BITMAP::SetX, &SCH_BITMAP::GetX,
                                     PROPERTY_DISPLAY::PT_COORD ) );


        propMgr.AddProperty( new PROPERTY<SCH_BITMAP, int>( _HKI( "Position Y" ),
                                     &SCH_BITMAP::SetY, &SCH_BITMAP::GetY,
                                     PROPERTY_DISPLAY::PT_COORD ) );

        const wxString groupImage = _HKI( "Image Properties" );

        propMgr.AddProperty( new PROPERTY<SCH_BITMAP, double>( _HKI( "Scale" ),
                                     &SCH_BITMAP::SetImageScale, &SCH_BITMAP::GetImageScale ),
                             groupImage );

        propMgr.AddProperty( new PROPERTY<SCH_BITMAP, int>( _HKI( "Transform Offset X" ),
                                     &SCH_BITMAP::SetTransformOriginOffsetX,
                                     &SCH_BITMAP::GetTransformOriginOffsetX,
                                     PROPERTY_DISPLAY::PT_COORD, ORIGIN_TRANSFORMS::ABS_X_COORD ),
                             groupImage );

        propMgr.AddProperty( new PROPERTY<SCH_BITMAP, int>( _HKI( "Transform Offset Y" ),
                                     &SCH_BITMAP::SetTransformOriginOffsetY,
                                     &SCH_BITMAP::GetTransformOriginOffsetY,
                                     PROPERTY_DISPLAY::PT_COORD, ORIGIN_TRANSFORMS::ABS_Y_COORD ),
                             groupImage );

        propMgr.AddProperty( new PROPERTY<SCH_BITMAP, int>( _HKI( "Width" ),
                                     &SCH_BITMAP::SetWidth, &SCH_BITMAP::GetWidth,
                                     PROPERTY_DISPLAY::PT_COORD ),
                             groupImage );

        propMgr.AddProperty( new PROPERTY<SCH_BITMAP, int>( _HKI( "Height" ),
                                     &SCH_BITMAP::SetHeight, &SCH_BITMAP::GetHeight,
                                     PROPERTY_DISPLAY::PT_COORD ),
                             groupImage );
    }
} _SCH_BITMAP_DESC;
