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

#ifndef SCH_EASYEDAPRO_PARSER_H_
#define SCH_EASYEDAPRO_PARSER_H_

#include <io/easyedapro/easyedapro_parser.h>

#include <sch_io/sch_io_mgr.h>
#include <pin_type.h>
#include <layer_ids.h>
#include <plotters/plotter.h>


class EDA_TEXT;
class SCH_PIN;
class SCH_SYMBOL;

namespace EASYEDAPRO
{
/**
 * Infer a KiCad electrical pin type from a pin name for symbols whose source
 * file provides no usable type information (LCSC/EasyEDA Pro exports commonly
 * set every pin to "Undefined"). Returns PT_UNSPECIFIED when the name is not
 * recognized.
 */
ELECTRICAL_PINTYPE GuessPinTypeFromName( const wxString& aPinName );

/**
 * Connector heuristic: a symbol is treated as a passive interface (receptacle,
 * cable, header) either when its name matches typical connector keywords
 * (USB, TYPE-C, CONNECTOR, ...) or when it contains no driving pins at all
 * (no power outputs, outputs or open collectors). Supply-rail pins of such
 * symbols (VBUS/VIN/VCC...) cannot be driven from within this sheet, so they
 * are demoted from power input to passive to avoid spurious "power input not
 * driven" errors on dual-role (DRP) power pins. Ground and exposed-pad pins
 * keep the power input type.
 */
void ApplyConnectorHeuristic( const std::vector<SCH_PIN*>& aPins, const wxString& aSymbolName,
                              bool aIsPowerPort );

bool IsConnectorSymbolName( const wxString& aName );

struct PIN_INFO
{
    EASYEDAPRO::SYM_PIN pin;
    wxString            number;
    wxString            name;
};

struct SYM_INFO
{
    EASYEDAPRO::SYM_HEAD                head;
    std::vector<PIN_INFO>               pins;
    std::unique_ptr<LIB_SYMBOL>         libSymbol;
    std::optional<EASYEDAPRO::SCH_ATTR> symbolAttr;
    std::map<wxString, int>             partUnits;
};
} // namespace EASYEDAPRO


class SCH_EASYEDAPRO_PARSER
{

public:
    explicit SCH_EASYEDAPRO_PARSER( SCHEMATIC* aSchematic, PROGRESS_REPORTER* aProgressReporter );
    ~SCH_EASYEDAPRO_PARSER();

    /*void Parse( const ALTIUM_COMPOUND_FILE&                  aAltiumPcbFile,
                const std::map<ALTIUM_PCB_DIR, std::string>& aFileMapping );*/

    static double Convert( wxString aValue );

    template <typename T>
    static T ScaleSize( T aValue )
    {
        return schIUScale.MilsToIU( KiROUND( aValue * 10 ) );
    }

    template <typename T>
    static VECTOR2<T> ScaleSize( VECTOR2<T> aValue )
    {
        return VECTOR2<T>( ScaleSize( aValue.x ), ScaleSize( aValue.y ) );
    }

    template <typename T>
    static VECTOR2<T> ScalePos( VECTOR2<T> aValue )
    {
        return VECTOR2<T>( ScaleSize( aValue.x ), -ScaleSize( aValue.y ) );
    }

    template <typename T>
    static VECTOR2<T> ScalePosSym( VECTOR2<T> aValue )
    {
        return VECTOR2<T>( ScaleSize( aValue.x ), -ScaleSize( aValue.y ) );
    }

    double SizeToKi( wxString units );

    EASYEDAPRO::SYM_INFO ParseSymbol( const std::vector<nlohmann::json>&  aLines,
                                      const std::map<wxString, wxString>& aDeviceAttributes );

    void ParseSchematic( SCHEMATIC* aSchematic, SCH_SHEET* aRootSheet,
                         const nlohmann::json&                       aProject,
                         std::map<wxString, EASYEDAPRO::SYM_INFO>&   aSymbolMap,
                         const std::map<wxString, EASYEDAPRO::BLOB>& aBlobMap,
                         const std::vector<nlohmann::json>& aLines, const wxString& aLibName );

protected:
    SCHEMATIC* m_schematic;

    template <typename T>
    void ApplyFontStyle( const std::map<wxString, nlohmann::json>& fontStyles, T& text,
                         const wxString& styleStr );

    template <typename T>
    void ApplyLineStyle( const std::map<wxString, nlohmann::json>& lineStyles, T& shape,
                         const wxString& styleStr );

    template <typename T>
    void ApplyAttrToField( const std::map<wxString, nlohmann::json>& fontStyles, T* text,
                           const EASYEDAPRO::SCH_ATTR& aAttr, bool aIsSym, bool aToSym,
                           const std::map<wxString, wxString>& aDeviceAttributes = {},
                           SCH_SYMBOL*                         aParent = nullptr );
};


#endif // SCH_EASYEDAPRO_PARSER_H_
