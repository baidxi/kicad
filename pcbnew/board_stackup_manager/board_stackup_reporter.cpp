/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2019 Jean-Pierre Charras, jp.charras at wanadoo.fr
 * Copyright (C) 2026 Krishna Swaroop <krishna.swaroop@pixxel.co.in>
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
  * @file board_stackup_reporter.cpp
  */

#include "wx/string.h"

#include <base_units.h>
#include <locale_io.h>

#include "board_stackup.h"
#include "stackup_predefined_prms.h"

#include "board_stackup_reporter.h"


static wxString csvQuote( const wxString& aValue )
{
    wxString escaped = aValue;
    escaped.Replace( wxT( "\"" ), wxT( "\"\"" ) );
    return wxT( "\"" ) + escaped + wxT( "\"" );
}


static void appendCsvField( wxString& aRow, const wxString& aValue )
{
    if( !aRow.IsEmpty() )
        aRow << wxT( ',' );

    aRow << csvQuote( aValue );
}


static wxString layerDisplayName( const BOARD_STACKUP_ITEM* aItem )
{
    if( aItem->GetType() == BS_ITEM_TYPE_DIELECTRIC )
        return aItem->FormatDielectricLayerName();

    return aItem->GetLayerName();
}


static void appendFinishAndOptions( wxString& aOut, BOARD_STACKUP& aStackup, const STACKUP_REPORT_OPTIONS& aOptions,
                                    bool aCsv )
{
    auto appendProperty = [&]( const wxString& aName, const wxString& aValue )
    {
        if( aCsv )
        {
            wxString row;
            appendCsvField( row, aName );
            appendCsvField( row, aValue );
            aOut << row << '\n';
        }
        else
        {
            // Report format mirrors the historical clipboard text.
            if( aName == wxT( "Finish" ) )
                aOut << wxString::Format( wxT( "Finish \"%s\"" ), aValue );
            else if( aName == wxT( "EdgeConnector" ) )
                aOut << wxString::Format( wxT( " EdgeConnector \"%s\"" ), aValue );
            else
                aOut << wxString::Format( wxT( " Option \"%s\"" ), aName );
        }
    };

    if( aOptions.includeFinish )
        appendProperty( wxT( "Finish" ), aStackup.m_FinishType );

    if( aOptions.includeBoardOptions )
    {
        if( aStackup.m_HasDielectricConstrains )
            appendProperty( wxT( "Impedance Controlled" ), wxT( "yes" ) );

        if( aStackup.m_EdgePlating )
            appendProperty( wxT( "Plated edges" ), wxT( "yes" ) );

        if( aStackup.m_EdgeConnectorConstraints != BS_EDGE_CONNECTOR_NONE )
        {
            wxString conn = wxT( "yes" );

            if( aStackup.m_EdgeConnectorConstraints == BS_EDGE_CONNECTOR_BEVELLED )
                conn << wxT( ",bevelled" );

            appendProperty( wxT( "EdgeConnector" ), conn );
        }
    }
}


wxString BuildStackupReport( BOARD_STACKUP& aStackup, EDA_UNITS aUnits )
{
    return BuildStackupReport( aStackup, aUnits, STACKUP_REPORT_OPTIONS() );
}


wxString BuildStackupReport( BOARD_STACKUP& aStackup, EDA_UNITS aUnits, const STACKUP_REPORT_OPTIONS& aOptions )
{
    wxString  report;
    wxString  txt;
    LOCALE_IO toggle;

    for( const BOARD_STACKUP_ITEM* item : aStackup.GetList() )
    {
        if( !item->IsEnabled() )
            continue;

        if( item->GetType() == BS_ITEM_TYPE_DIELECTRIC )
        {
            wxString sublayer_text;

            if( item->GetSublayersCount() )
            {
                sublayer_text.Printf( wxT( "\n  sublayer \"1/%d\"" ), item->GetSublayersCount() );
            }

            txt.Printf( wxT( "layer \"%s\" type \"%s\"%s" ), item->FormatDielectricLayerName(), item->GetTypeName(),
                        sublayer_text );
        }
        else
        {
            txt.Printf( wxT( "layer \"%s\" type \"%s\"" ), item->GetLayerName(), item->GetTypeName() );
        }

        report << txt;

        if( aOptions.includeColor && item->IsColorEditable() )
            report << wxString::Format( wxT( " Color \"%s\"" ), item->GetColor() );

        for( int idx = 0; idx < item->GetSublayersCount(); idx++ )
        {
            if( idx )
            {
                report << wxString::Format( wxT( "\n  sublayer \"%d/%d\"" ), idx + 1, item->GetSublayersCount() );
            }

            if( aOptions.includeThickness && item->IsThicknessEditable() )
            {
                report << wxString::Format(
                        wxT( " Thickness %s" ),
                        EDA_UNIT_UTILS::UI::StringFromValue( pcbIUScale, aUnits, item->GetThickness( idx ), true ) );

                if( item->GetType() == BS_ITEM_TYPE_DIELECTRIC && item->IsThicknessLocked( idx ) )
                    report << wxT( " Locked" );
            }

            if( aOptions.includeMaterial && item->IsMaterialEditable() )
                report << wxString::Format( wxT( " Material \"%s\"" ), item->GetMaterial( idx ) );

            if( aOptions.includeEpsilonR && item->HasEpsilonRValue() )
                report << wxString::Format( wxT( " EpsilonR %s" ), item->FormatEpsilonR( idx ) );

            if( aOptions.includeLossTangent && item->HasLossTangentValue() )
                report << wxString::Format( wxT( " LossTg %s" ), item->FormatLossTangent( idx ) );
        }

        report << '\n';
    }

    appendFinishAndOptions( report, aStackup, aOptions, false );
    report << '\n';
    return report;
}


wxString BuildStackupCsv( BOARD_STACKUP& aStackup, EDA_UNITS aUnits, const STACKUP_REPORT_OPTIONS& aOptions )
{
    wxString  csv;
    LOCALE_IO toggle;

    // Fixed columns, then optional ones in a stable order.
    wxString header;
    appendCsvField( header, wxT( "Layer" ) );
    appendCsvField( header, wxT( "Type" ) );
    appendCsvField( header, wxT( "Sublayer" ) );

    if( aOptions.includeThickness )
    {
        appendCsvField( header, wxT( "Thickness" ) );
        appendCsvField( header, wxT( "Locked" ) );
    }

    if( aOptions.includeMaterial )
        appendCsvField( header, wxT( "Material" ) );

    if( aOptions.includeColor )
        appendCsvField( header, wxT( "Color" ) );

    if( aOptions.includeEpsilonR )
        appendCsvField( header, wxT( "EpsilonR" ) );

    if( aOptions.includeLossTangent )
        appendCsvField( header, wxT( "LossTg" ) );

    csv << header << '\n';

    for( const BOARD_STACKUP_ITEM* item : aStackup.GetList() )
    {
        if( !item->IsEnabled() )
            continue;

        int sublayerCount = item->GetSublayersCount();

        if( sublayerCount <= 0 )
            sublayerCount = 1;

        for( int idx = 0; idx < sublayerCount; idx++ )
        {
            const bool hasSublayerData = item->GetSublayersCount() > 0;
            wxString   row;

            appendCsvField( row, layerDisplayName( item ) );
            appendCsvField( row, item->GetTypeName() );

            if( item->GetType() == BS_ITEM_TYPE_DIELECTRIC && item->GetSublayersCount() > 1 )
            {
                appendCsvField( row, wxString::Format( wxT( "%d/%d" ), idx + 1, item->GetSublayersCount() ) );
            }
            else
            {
                appendCsvField( row, wxEmptyString );
            }

            if( aOptions.includeThickness )
            {
                wxString thickness;

                if( item->IsThicknessEditable() && hasSublayerData )
                {
                    thickness =
                            EDA_UNIT_UTILS::UI::StringFromValue( pcbIUScale, aUnits, item->GetThickness( idx ), true );
                }

                appendCsvField( row, thickness );

                const bool locked =
                        item->GetType() == BS_ITEM_TYPE_DIELECTRIC && hasSublayerData && item->IsThicknessLocked( idx );
                appendCsvField( row, locked ? wxT( "yes" ) : wxEmptyString );
            }

            if( aOptions.includeMaterial )
            {
                appendCsvField( row, ( item->IsMaterialEditable() && hasSublayerData ) ? item->GetMaterial( idx )
                                                                                       : wxString() );
            }

            if( aOptions.includeColor )
            {
                appendCsvField( row, item->IsColorEditable() ? item->GetColor() : wxString() );
            }

            if( aOptions.includeEpsilonR )
            {
                appendCsvField( row, ( item->HasEpsilonRValue() && hasSublayerData ) ? item->FormatEpsilonR( idx )
                                                                                     : wxString() );
            }

            if( aOptions.includeLossTangent )
            {
                appendCsvField( row, ( item->HasLossTangentValue() && hasSublayerData ) ? item->FormatLossTangent( idx )
                                                                                        : wxString() );
            }

            csv << row << '\n';
        }
    }

    if( aOptions.includeFinish || aOptions.includeBoardOptions )
    {
        csv << '\n';

        wxString propHeader;
        appendCsvField( propHeader, wxT( "Property" ) );
        appendCsvField( propHeader, wxT( "Value" ) );
        csv << propHeader << '\n';

        appendFinishAndOptions( csv, aStackup, aOptions, true );
    }

    return csv;
}
