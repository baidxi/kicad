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

#include "plm_diff_pcb.h"

#include <wx/file.h>
#include <wx/filename.h>
#include <wx/log.h>
#include <wx/regex.h>

#include <algorithm>
#include <map>


namespace
{

wxString ExtractSExprValue( const wxString& aContent, const wxString& aKey )
{
    wxString pattern = wxS( "\\(" ) + aKey + wxS( "\\s+\"?([^\")]+)\"?\\)" );
    wxRegEx regex( pattern );

    if( regex.Matches( aContent ) )
        return regex.GetMatch( aContent, 1 );

    return wxEmptyString;
}


wxString ExtractUUID( const wxString& aContent )
{
    wxRegEx uuidRegex( wxS( "\\(uuid\\s+\"?([a-f0-9-]+)\"?\\)" ) );

    if( uuidRegex.Matches( aContent ) )
        return uuidRegex.GetMatch( aContent, 1 );

    return wxEmptyString;
}


bool ExtractPosition( const wxString& aContent, int32_t& aX, int32_t& aY, int32_t& aRotation )
{
    wxRegEx atRegex( wxS( "\\(at\\s+([0-9.-]+)\\s+([0-9.-]+)(?:\\s+([0-9.-]+))?\\)" ) );

    if( atRegex.Matches( aContent ) )
    {
        double x, y, rot = 0;
        atRegex.GetMatch( aContent, 1 ).ToDouble( &x );
        atRegex.GetMatch( aContent, 2 ).ToDouble( &y );

        wxString rotStr = atRegex.GetMatch( aContent, 3 );

        if( !rotStr.IsEmpty() )
            rotStr.ToDouble( &rot );

        aX = static_cast<int32_t>( x * 1000000 );
        aY = static_cast<int32_t>( y * 1000000 );
        aRotation = static_cast<int32_t>( rot * 10 );
        return true;
    }

    return false;
}


bool ExtractStartEnd( const wxString& aContent, PLM_PCB_POINT& aStart, PLM_PCB_POINT& aEnd )
{
    wxRegEx startRegex( wxS( "\\(start\\s+([0-9.-]+)\\s+([0-9.-]+)\\)" ) );
    wxRegEx endRegex( wxS( "\\(end\\s+([0-9.-]+)\\s+([0-9.-]+)\\)" ) );

    bool hasStart = false;
    bool hasEnd = false;

    if( startRegex.Matches( aContent ) )
    {
        double x, y;
        startRegex.GetMatch( aContent, 1 ).ToDouble( &x );
        startRegex.GetMatch( aContent, 2 ).ToDouble( &y );
        aStart.x = static_cast<int32_t>( x * 1000000 );
        aStart.y = static_cast<int32_t>( y * 1000000 );
        hasStart = true;
    }

    if( endRegex.Matches( aContent ) )
    {
        double x, y;
        endRegex.GetMatch( aContent, 1 ).ToDouble( &x );
        endRegex.GetMatch( aContent, 2 ).ToDouble( &y );
        aEnd.x = static_cast<int32_t>( x * 1000000 );
        aEnd.y = static_cast<int32_t>( y * 1000000 );
        hasEnd = true;
    }

    return hasStart && hasEnd;
}


wxString ReadFileContent( const wxString& aFilename )
{
    if( !wxFileName::FileExists( aFilename ) )
        return wxEmptyString;

    wxFile file( aFilename );

    if( !file.IsOpened() )
        return wxEmptyString;

    wxString content;
    file.ReadAll( &content );
    return content;
}


std::vector<wxString> FindSExprBlocks( const wxString& aContent, const wxString& aBlockType )
{
    std::vector<wxString> blocks;
    wxString searchStart = wxS( "(" ) + aBlockType + wxS( " " );

    size_t pos = 0;

    while( ( pos = aContent.find( searchStart, pos ) ) != wxString::npos )
    {
        int depth = 1;
        size_t start = pos;
        pos += searchStart.length();

        while( pos < aContent.length() && depth > 0 )
        {
            if( aContent[pos] == '(' )
                ++depth;
            else if( aContent[pos] == ')' )
                --depth;

            ++pos;
        }

        if( depth == 0 )
            blocks.push_back( aContent.Mid( start, pos - start ) );
    }

    return blocks;
}

} // anonymous namespace


PLM_PCB_DIFF_PARSER::PLM_PCB_DIFF_PARSER()
{
}


PLM_PCB_DIFF_PARSER::~PLM_PCB_DIFF_PARSER()
{
}


PLM_PCB_DATA PLM_PCB_DIFF_PARSER::ParsePCB( const wxString& aFilename )
{
    PLM_PCB_DATA data;
    m_lastError.clear();

    wxString content = ReadFileContent( aFilename );

    if( content.IsEmpty() )
    {
        m_lastError = wxString::Format( wxS( "Failed to read PCB file: %s" ), aFilename );
        return data;
    }

    data.filename = aFilename;
    data.board_uuid = ExtractUUID( content );

    // Parse layer count
    wxRegEx layerCountRegex( wxS( "\\(layers\\s+([0-9]+)\\)" ) );

    if( layerCountRegex.Matches( content ) )
    {
        long layers;
        layerCountRegex.GetMatch( content, 1 ).ToLong( &layers );
        data.layer_count = static_cast<int32_t>( layers );
    }

    // Parse footprints
    std::vector<wxString> footprintBlocks = FindSExprBlocks( content, wxS( "footprint" ) );

    for( const wxString& block : footprintBlocks )
    {
        PLM_PCB_FOOTPRINT fp = ParseFootprint( block );
        data.footprints.push_back( fp );
    }

    // Parse segments (tracks)
    std::vector<wxString> segmentBlocks = FindSExprBlocks( content, wxS( "segment" ) );

    for( const wxString& block : segmentBlocks )
    {
        PLM_PCB_TRACK track = ParseTrack( block );
        data.tracks.push_back( track );
    }

    // Parse arcs
    std::vector<wxString> arcBlocks = FindSExprBlocks( content, wxS( "arc" ) );

    for( const wxString& block : arcBlocks )
    {
        PLM_PCB_TRACK arc = ParseTrack( block );
        data.tracks.push_back( arc );
    }

    // Parse vias
    std::vector<wxString> viaBlocks = FindSExprBlocks( content, wxS( "via" ) );

    for( const wxString& block : viaBlocks )
    {
        PLM_PCB_VIA via = ParseVia( block );
        data.vias.push_back( via );
    }

    // Parse zones
    std::vector<wxString> zoneBlocks = FindSExprBlocks( content, wxS( "zone" ) );

    for( const wxString& block : zoneBlocks )
    {
        PLM_PCB_ZONE zone = ParseZone( block );
        data.zones.push_back( zone );
    }

    // Parse graphic lines
    std::vector<wxString> grLineBlocks = FindSExprBlocks( content, wxS( "gr_line" ) );

    for( const wxString& block : grLineBlocks )
    {
        PLM_PCB_GRAPHIC graphic;
        graphic.uuid = ExtractUUID( block );
        graphic.type = PLM_PCB_ELEMENT_TYPE::GRAPHIC_LINE;

        wxString layerName = ExtractSExprValue( block, wxS( "layer" ) );
        graphic.layer = ParseLayer( layerName );

        ExtractStartEnd( block, graphic.start, graphic.end );

        wxRegEx widthRegex( wxS( "\\(stroke\\s+\\(width\\s+([0-9.-]+)\\)" ) );

        if( widthRegex.Matches( block ) )
        {
            double w;
            widthRegex.GetMatch( block, 1 ).ToDouble( &w );
            graphic.width = static_cast<int32_t>( w * 1000000 );
        }

        data.graphics.push_back( graphic );
    }

    // Parse graphic rectangles
    std::vector<wxString> grRectBlocks = FindSExprBlocks( content, wxS( "gr_rect" ) );

    for( const wxString& block : grRectBlocks )
    {
        PLM_PCB_GRAPHIC graphic;
        graphic.uuid = ExtractUUID( block );
        graphic.type = PLM_PCB_ELEMENT_TYPE::GRAPHIC_RECT;

        wxString layerName = ExtractSExprValue( block, wxS( "layer" ) );
        graphic.layer = ParseLayer( layerName );

        ExtractStartEnd( block, graphic.start, graphic.end );

        data.graphics.push_back( graphic );
    }

    // Parse graphic circles
    std::vector<wxString> grCircleBlocks = FindSExprBlocks( content, wxS( "gr_circle" ) );

    for( const wxString& block : grCircleBlocks )
    {
        PLM_PCB_GRAPHIC graphic;
        graphic.uuid = ExtractUUID( block );
        graphic.type = PLM_PCB_ELEMENT_TYPE::GRAPHIC_CIRCLE;

        wxString layerName = ExtractSExprValue( block, wxS( "layer" ) );
        graphic.layer = ParseLayer( layerName );

        wxRegEx centerRegex( wxS( "\\(center\\s+([0-9.-]+)\\s+([0-9.-]+)\\)" ) );

        if( centerRegex.Matches( block ) )
        {
            double x, y;
            centerRegex.GetMatch( block, 1 ).ToDouble( &x );
            centerRegex.GetMatch( block, 2 ).ToDouble( &y );
            graphic.start.x = static_cast<int32_t>( x * 1000000 );
            graphic.start.y = static_cast<int32_t>( y * 1000000 );
        }

        data.graphics.push_back( graphic );
    }

    // Parse text annotations
    std::vector<wxString> grTextBlocks = FindSExprBlocks( content, wxS( "gr_text" ) );

    for( const wxString& block : grTextBlocks )
    {
        PLM_PCB_TEXT text;
        text.uuid = ExtractUUID( block );

        wxRegEx textRegex( wxS( "\\(gr_text\\s+\"([^\"]+)\"" ) );

        if( textRegex.Matches( block ) )
            text.text = textRegex.GetMatch( block, 1 );

        wxString layerName = ExtractSExprValue( block, wxS( "layer" ) );
        text.layer = ParseLayer( layerName );

        int32_t x, y, rot;

        if( ExtractPosition( block, x, y, rot ) )
        {
            text.geometry.x = x;
            text.geometry.y = y;
            text.geometry.rotation = rot;
        }

        data.texts.push_back( text );
    }

    return data;
}


PLM_PCB_FOOTPRINT PLM_PCB_DIFF_PARSER::ParseFootprint( const wxString& aContent )
{
    PLM_PCB_FOOTPRINT fp;

    fp.uuid = ExtractUUID( aContent );

    wxRegEx libIdRegex( wxS( "\\(footprint\\s+\"([^\"]+)\"" ) );

    if( libIdRegex.Matches( aContent ) )
        fp.lib_id = libIdRegex.GetMatch( aContent, 1 );

    int32_t x, y, rot;

    if( ExtractPosition( aContent, x, y, rot ) )
    {
        fp.geometry.x = x;
        fp.geometry.y = y;
        fp.geometry.rotation = rot;
    }

    wxString layerName = ExtractSExprValue( aContent, wxS( "layer" ) );
    fp.layer = ParseLayer( layerName );
    fp.geometry.flipped = ( fp.layer == PLM_PCB_LAYER::B_CU );

    // Extract properties (Reference, Value)
    std::vector<wxString> propertyBlocks = FindSExprBlocks( aContent, wxS( "property" ) );

    for( const wxString& propBlock : propertyBlocks )
    {
        wxRegEx propRegex( wxS( "\\(property\\s+\"([^\"]+)\"\\s+\"([^\"]*)\"" ) );

        if( propRegex.Matches( propBlock ) )
        {
            wxString propName = propRegex.GetMatch( propBlock, 1 );
            wxString propValue = propRegex.GetMatch( propBlock, 2 );

            if( propName == wxS( "Reference" ) )
                fp.reference = propValue;
            else if( propName == wxS( "Value" ) )
                fp.value = propValue;
            else
                fp.properties[propName] = propValue;
        }
    }

    // Also check for fp_text reference and value (older format)
    std::vector<wxString> fpTextBlocks = FindSExprBlocks( aContent, wxS( "fp_text" ) );

    for( const wxString& textBlock : fpTextBlocks )
    {
        wxRegEx textTypeRegex( wxS( "\\(fp_text\\s+(reference|value)\\s+\"([^\"]*)\"" ) );

        if( textTypeRegex.Matches( textBlock ) )
        {
            wxString textType = textTypeRegex.GetMatch( textBlock, 1 );
            wxString textValue = textTypeRegex.GetMatch( textBlock, 2 );

            if( textType == wxS( "reference" ) && fp.reference.IsEmpty() )
                fp.reference = textValue;
            else if( textType == wxS( "value" ) && fp.value.IsEmpty() )
                fp.value = textValue;
        }
    }

    return fp;
}


PLM_PCB_TRACK PLM_PCB_DIFF_PARSER::ParseTrack( const wxString& aContent )
{
    PLM_PCB_TRACK track;

    track.uuid = ExtractUUID( aContent );

    ExtractStartEnd( aContent, track.start, track.end );

    wxRegEx widthRegex( wxS( "\\(width\\s+([0-9.-]+)\\)" ) );

    if( widthRegex.Matches( aContent ) )
    {
        double w;
        widthRegex.GetMatch( aContent, 1 ).ToDouble( &w );
        track.width = static_cast<int32_t>( w * 1000000 );
    }

    wxString layerName = ExtractSExprValue( aContent, wxS( "layer" ) );
    track.layer = ParseLayer( layerName );

    track.net = ExtractSExprValue( aContent, wxS( "net" ) );

    return track;
}


PLM_PCB_VIA PLM_PCB_DIFF_PARSER::ParseVia( const wxString& aContent )
{
    PLM_PCB_VIA via;

    via.uuid = ExtractUUID( aContent );

    wxRegEx atRegex( wxS( "\\(at\\s+([0-9.-]+)\\s+([0-9.-]+)\\)" ) );

    if( atRegex.Matches( aContent ) )
    {
        double x, y;
        atRegex.GetMatch( aContent, 1 ).ToDouble( &x );
        atRegex.GetMatch( aContent, 2 ).ToDouble( &y );
        via.position.x = static_cast<int32_t>( x * 1000000 );
        via.position.y = static_cast<int32_t>( y * 1000000 );
    }

    wxRegEx sizeRegex( wxS( "\\(size\\s+([0-9.-]+)\\)" ) );

    if( sizeRegex.Matches( aContent ) )
    {
        double s;
        sizeRegex.GetMatch( aContent, 1 ).ToDouble( &s );
        via.size = static_cast<int32_t>( s * 1000000 );
    }

    wxRegEx drillRegex( wxS( "\\(drill\\s+([0-9.-]+)\\)" ) );

    if( drillRegex.Matches( aContent ) )
    {
        double d;
        drillRegex.GetMatch( aContent, 1 ).ToDouble( &d );
        via.drill = static_cast<int32_t>( d * 1000000 );
    }

    wxRegEx layersRegex( wxS( "\\(layers\\s+\"?([^\"\\s]+)\"?\\s+\"?([^\"\\s\\)]+)\"?\\)" ) );

    if( layersRegex.Matches( aContent ) )
    {
        via.start_layer = ParseLayer( layersRegex.GetMatch( aContent, 1 ) );
        via.end_layer = ParseLayer( layersRegex.GetMatch( aContent, 2 ) );
    }

    via.net = ExtractSExprValue( aContent, wxS( "net" ) );

    return via;
}


PLM_PCB_ZONE PLM_PCB_DIFF_PARSER::ParseZone( const wxString& aContent )
{
    PLM_PCB_ZONE zone;

    zone.uuid = ExtractUUID( aContent );
    zone.net = ExtractSExprValue( aContent, wxS( "net_name" ) );

    wxString layerName = ExtractSExprValue( aContent, wxS( "layer" ) );
    zone.layer = ParseLayer( layerName );

    wxRegEx priorityRegex( wxS( "\\(priority\\s+([0-9]+)\\)" ) );

    if( priorityRegex.Matches( aContent ) )
    {
        long p;
        priorityRegex.GetMatch( aContent, 1 ).ToLong( &p );
        zone.priority = static_cast<int32_t>( p );
    }

    // Parse polygon outline
    wxRegEx xyRegex( wxS( "\\(xy\\s+([0-9.-]+)\\s+([0-9.-]+)\\)" ) );
    size_t start = 0;

    while( xyRegex.Matches( aContent.Mid( start ) ) )
    {
        PLM_PCB_POINT pt;
        double x, y;
        xyRegex.GetMatch( aContent.Mid( start ), 1 ).ToDouble( &x );
        xyRegex.GetMatch( aContent.Mid( start ), 2 ).ToDouble( &y );
        pt.x = static_cast<int32_t>( x * 1000000 );
        pt.y = static_cast<int32_t>( y * 1000000 );
        zone.outline.push_back( pt );

        size_t matchStart, matchLen;
        xyRegex.GetMatch( &matchStart, &matchLen, 0 );
        start += matchStart + matchLen;
    }

    return zone;
}


PLM_PCB_LAYER PLM_PCB_DIFF_PARSER::ParseLayer( const wxString& aLayerName )
{
    static const std::map<wxString, PLM_PCB_LAYER> layerMap = {
        { wxS( "F.Cu" ), PLM_PCB_LAYER::F_CU },
        { wxS( "B.Cu" ), PLM_PCB_LAYER::B_CU },
        { wxS( "In1.Cu" ), PLM_PCB_LAYER::IN1_CU },
        { wxS( "In2.Cu" ), PLM_PCB_LAYER::IN2_CU },
        { wxS( "F.SilkS" ), PLM_PCB_LAYER::F_SILKS },
        { wxS( "B.SilkS" ), PLM_PCB_LAYER::B_SILKS },
        { wxS( "F.Mask" ), PLM_PCB_LAYER::F_MASK },
        { wxS( "B.Mask" ), PLM_PCB_LAYER::B_MASK },
        { wxS( "F.Paste" ), PLM_PCB_LAYER::F_PASTE },
        { wxS( "B.Paste" ), PLM_PCB_LAYER::B_PASTE },
        { wxS( "F.Adhes" ), PLM_PCB_LAYER::F_ADHES },
        { wxS( "B.Adhes" ), PLM_PCB_LAYER::B_ADHES },
        { wxS( "F.Fab" ), PLM_PCB_LAYER::F_FAB },
        { wxS( "B.Fab" ), PLM_PCB_LAYER::B_FAB },
        { wxS( "F.CrtYd" ), PLM_PCB_LAYER::F_CRTYD },
        { wxS( "B.CrtYd" ), PLM_PCB_LAYER::B_CRTYD },
        { wxS( "Edge.Cuts" ), PLM_PCB_LAYER::EDGE_CUTS },
        { wxS( "Margin" ), PLM_PCB_LAYER::MARGIN },
        { wxS( "User.1" ), PLM_PCB_LAYER::USER_1 }
    };

    auto it = layerMap.find( aLayerName );

    if( it != layerMap.end() )
        return it->second;

    return PLM_PCB_LAYER::UNKNOWN;
}


PLM_PCB_DIFF_RESULT PLM_PCB_DIFF_PARSER::ComputeDiff( const PLM_PCB_DATA& aBase,
                                                       const PLM_PCB_DATA& aNew )
{
    PLM_PCB_DIFF_RESULT result;
    result.base_file = aBase.filename;
    result.new_file = aNew.filename;
    result.success = true;

    // Build maps by UUID for quick lookup
    std::map<wxString, const PLM_PCB_FOOTPRINT*> baseFootprints;
    std::map<wxString, const PLM_PCB_FOOTPRINT*> newFootprints;

    for( const auto& fp : aBase.footprints )
        baseFootprints[fp.uuid] = &fp;

    for( const auto& fp : aNew.footprints )
        newFootprints[fp.uuid] = &fp;

    // Find added and modified footprints
    for( const auto& [uuid, newFp] : newFootprints )
    {
        auto baseIt = baseFootprints.find( uuid );

        if( baseIt == baseFootprints.end() )
        {
            PLM_PCB_CHANGE change;
            change.change_type = PLM_PCB_CHANGE_TYPE::ADDED;
            change.element_type = PLM_PCB_ELEMENT_TYPE::FOOTPRINT;
            change.uuid = uuid;
            change.layer = newFp->layer;
            change.description = wxString::Format( wxS( "Added footprint %s (%s)" ),
                                                   newFp->reference, newFp->lib_id );
            result.changes.push_back( change );
        }
        else
        {
            CompareFootprints( *baseIt->second, *newFp, result.changes );
        }
    }

    // Find removed footprints
    for( const auto& [uuid, baseFp] : baseFootprints )
    {
        if( newFootprints.find( uuid ) == newFootprints.end() )
        {
            PLM_PCB_CHANGE change;
            change.change_type = PLM_PCB_CHANGE_TYPE::REMOVED;
            change.element_type = PLM_PCB_ELEMENT_TYPE::FOOTPRINT;
            change.uuid = uuid;
            change.layer = baseFp->layer;
            change.description = wxString::Format( wxS( "Removed footprint %s (%s)" ),
                                                   baseFp->reference, baseFp->lib_id );
            result.changes.push_back( change );
        }
    }

    // Build track maps
    std::map<wxString, const PLM_PCB_TRACK*> baseTracks;
    std::map<wxString, const PLM_PCB_TRACK*> newTracks;

    for( const auto& track : aBase.tracks )
        baseTracks[track.uuid] = &track;

    for( const auto& track : aNew.tracks )
        newTracks[track.uuid] = &track;

    // Find added tracks
    for( const auto& [uuid, newTrack] : newTracks )
    {
        if( baseTracks.find( uuid ) == baseTracks.end() )
        {
            PLM_PCB_CHANGE change;
            change.change_type = PLM_PCB_CHANGE_TYPE::ADDED;
            change.element_type = PLM_PCB_ELEMENT_TYPE::TRACK;
            change.uuid = uuid;
            change.layer = newTrack->layer;
            change.description = wxS( "Added track segment" );
            result.changes.push_back( change );
        }
        else
        {
            CompareTracks( *baseTracks[uuid], *newTrack, result.changes );
        }
    }

    // Find removed tracks
    for( const auto& [uuid, baseTrack] : baseTracks )
    {
        if( newTracks.find( uuid ) == newTracks.end() )
        {
            PLM_PCB_CHANGE change;
            change.change_type = PLM_PCB_CHANGE_TYPE::REMOVED;
            change.element_type = PLM_PCB_ELEMENT_TYPE::TRACK;
            change.uuid = uuid;
            change.layer = baseTrack->layer;
            change.description = wxS( "Removed track segment" );
            result.changes.push_back( change );
        }
    }

    // Build via maps
    std::map<wxString, const PLM_PCB_VIA*> baseVias;
    std::map<wxString, const PLM_PCB_VIA*> newVias;

    for( const auto& via : aBase.vias )
        baseVias[via.uuid] = &via;

    for( const auto& via : aNew.vias )
        newVias[via.uuid] = &via;

    // Find added vias
    for( const auto& [uuid, newVia] : newVias )
    {
        if( baseVias.find( uuid ) == baseVias.end() )
        {
            PLM_PCB_CHANGE change;
            change.change_type = PLM_PCB_CHANGE_TYPE::ADDED;
            change.element_type = PLM_PCB_ELEMENT_TYPE::VIA;
            change.uuid = uuid;
            change.layer = PLM_PCB_LAYER::MULTI;
            change.description = wxS( "Added via" );
            result.changes.push_back( change );
        }
    }

    // Find removed vias
    for( const auto& [uuid, baseVia] : baseVias )
    {
        if( newVias.find( uuid ) == newVias.end() )
        {
            PLM_PCB_CHANGE change;
            change.change_type = PLM_PCB_CHANGE_TYPE::REMOVED;
            change.element_type = PLM_PCB_ELEMENT_TYPE::VIA;
            change.uuid = uuid;
            change.layer = PLM_PCB_LAYER::MULTI;
            change.description = wxS( "Removed via" );
            result.changes.push_back( change );
        }
    }

    // Build zone maps
    std::map<wxString, const PLM_PCB_ZONE*> baseZones;
    std::map<wxString, const PLM_PCB_ZONE*> newZones;

    for( const auto& zone : aBase.zones )
        baseZones[zone.uuid] = &zone;

    for( const auto& zone : aNew.zones )
        newZones[zone.uuid] = &zone;

    // Find added zones
    for( const auto& [uuid, newZone] : newZones )
    {
        if( baseZones.find( uuid ) == baseZones.end() )
        {
            PLM_PCB_CHANGE change;
            change.change_type = PLM_PCB_CHANGE_TYPE::ADDED;
            change.element_type = PLM_PCB_ELEMENT_TYPE::ZONE;
            change.uuid = uuid;
            change.layer = newZone->layer;
            change.description = wxString::Format( wxS( "Added zone (%s)" ), newZone->net );
            result.changes.push_back( change );
        }
    }

    // Find removed zones
    for( const auto& [uuid, baseZone] : baseZones )
    {
        if( newZones.find( uuid ) == newZones.end() )
        {
            PLM_PCB_CHANGE change;
            change.change_type = PLM_PCB_CHANGE_TYPE::REMOVED;
            change.element_type = PLM_PCB_ELEMENT_TYPE::ZONE;
            change.uuid = uuid;
            change.layer = baseZone->layer;
            change.description = wxString::Format( wxS( "Removed zone (%s)" ), baseZone->net );
            result.changes.push_back( change );
        }
    }

    return result;
}


void PLM_PCB_DIFF_PARSER::CompareFootprints( const PLM_PCB_FOOTPRINT& aBase,
                                               const PLM_PCB_FOOTPRINT& aNew,
                                               std::vector<PLM_PCB_CHANGE>& aChanges )
{
    if( aBase == aNew )
        return;

    bool positionOnly = ( aBase.lib_id == aNew.lib_id && aBase.reference == aNew.reference &&
                          aBase.value == aNew.value && aBase.layer == aNew.layer &&
                          aBase.properties == aNew.properties );

    if( positionOnly && aBase.geometry != aNew.geometry )
    {
        PLM_PCB_CHANGE change;
        change.change_type = PLM_PCB_CHANGE_TYPE::MOVED;
        change.element_type = PLM_PCB_ELEMENT_TYPE::FOOTPRINT;
        change.uuid = aNew.uuid;
        change.layer = aNew.layer;
        change.description = wxString::Format( wxS( "Moved footprint %s" ), aNew.reference );
        aChanges.push_back( change );
        return;
    }

    if( aBase.reference != aNew.reference )
    {
        PLM_PCB_CHANGE change;
        change.change_type = PLM_PCB_CHANGE_TYPE::MODIFIED;
        change.element_type = PLM_PCB_ELEMENT_TYPE::FOOTPRINT;
        change.uuid = aNew.uuid;
        change.layer = aNew.layer;
        change.description = wxS( "Changed reference designator" );
        change.old_value = aBase.reference;
        change.new_value = aNew.reference;
        aChanges.push_back( change );
    }

    if( aBase.value != aNew.value )
    {
        PLM_PCB_CHANGE change;
        change.change_type = PLM_PCB_CHANGE_TYPE::MODIFIED;
        change.element_type = PLM_PCB_ELEMENT_TYPE::FOOTPRINT;
        change.uuid = aNew.uuid;
        change.layer = aNew.layer;
        change.description = wxString::Format( wxS( "Changed value of %s" ), aNew.reference );
        change.old_value = aBase.value;
        change.new_value = aNew.value;
        aChanges.push_back( change );
    }

    if( aBase.lib_id != aNew.lib_id )
    {
        PLM_PCB_CHANGE change;
        change.change_type = PLM_PCB_CHANGE_TYPE::MODIFIED;
        change.element_type = PLM_PCB_ELEMENT_TYPE::FOOTPRINT;
        change.uuid = aNew.uuid;
        change.layer = aNew.layer;
        change.description = wxString::Format( wxS( "Changed footprint type of %s" ),
                                               aNew.reference );
        change.old_value = aBase.lib_id;
        change.new_value = aNew.lib_id;
        aChanges.push_back( change );
    }

    if( aBase.layer != aNew.layer )
    {
        PLM_PCB_CHANGE change;
        change.change_type = PLM_PCB_CHANGE_TYPE::MODIFIED;
        change.element_type = PLM_PCB_ELEMENT_TYPE::FOOTPRINT;
        change.uuid = aNew.uuid;
        change.layer = aNew.layer;
        change.description = wxString::Format( wxS( "Moved %s to other side" ), aNew.reference );
        aChanges.push_back( change );
    }
}


void PLM_PCB_DIFF_PARSER::CompareTracks( const PLM_PCB_TRACK& aBase, const PLM_PCB_TRACK& aNew,
                                           std::vector<PLM_PCB_CHANGE>& aChanges )
{
    if( aBase == aNew )
        return;

    if( aBase.start != aNew.start || aBase.end != aNew.end )
    {
        PLM_PCB_CHANGE change;
        change.change_type = PLM_PCB_CHANGE_TYPE::MODIFIED;
        change.element_type = PLM_PCB_ELEMENT_TYPE::TRACK;
        change.uuid = aNew.uuid;
        change.layer = aNew.layer;
        change.description = wxS( "Modified track endpoints" );
        aChanges.push_back( change );
    }

    if( aBase.width != aNew.width )
    {
        PLM_PCB_CHANGE change;
        change.change_type = PLM_PCB_CHANGE_TYPE::MODIFIED;
        change.element_type = PLM_PCB_ELEMENT_TYPE::TRACK;
        change.uuid = aNew.uuid;
        change.layer = aNew.layer;
        change.description = wxS( "Changed track width" );
        aChanges.push_back( change );
    }

    if( aBase.layer != aNew.layer )
    {
        PLM_PCB_CHANGE change;
        change.change_type = PLM_PCB_CHANGE_TYPE::MODIFIED;
        change.element_type = PLM_PCB_ELEMENT_TYPE::TRACK;
        change.uuid = aNew.uuid;
        change.layer = aNew.layer;
        change.description = wxS( "Changed track layer" );
        aChanges.push_back( change );
    }

    if( aBase.net != aNew.net )
    {
        PLM_PCB_CHANGE change;
        change.change_type = PLM_PCB_CHANGE_TYPE::MODIFIED;
        change.element_type = PLM_PCB_ELEMENT_TYPE::TRACK;
        change.uuid = aNew.uuid;
        change.layer = aNew.layer;
        change.description = wxS( "Changed track net" );
        change.old_value = aBase.net;
        change.new_value = aNew.net;
        aChanges.push_back( change );
    }
}
