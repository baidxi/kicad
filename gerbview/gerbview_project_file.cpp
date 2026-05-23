/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright The KiCad Developers, see AUTHORS.txt for contributors.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gerbview_project_file.h"

#include <fstream>

#include <json_common.h>
#include <lset.h>
#include <reporter.h>
#include <wx/filename.h>
#include <wx/translation.h>

#include "gerber_file_image.h"
#include "excellon_image.h"
#include "gerber_file_image_list.h"
#include "gerbview_frame.h"
#include "gerbview_settings.h"
#include "layer_roles.h"


namespace
{

constexpr const char* kKeySchemaVersion = "schema_version";
constexpr const char* kKeyKind = "kind";
constexpr const char* kKeyGenerator = "generator";
constexpr const char* kKeyFiles = "files";
constexpr const char* kKeyFilePath = "path";
constexpr const char* kKeyFileKind = "kind";
constexpr const char* kKeyFileRole = "role";
constexpr const char* kKeyFileVisible = "visible";
constexpr const char* kKeyColors = "colors";
constexpr const char* kKeySettings = "settings";
constexpr const char* kKeyVisibleInnerCount = "visible_inner_count";
constexpr const char* kKeyColorSchemeName = "color_scheme_name";

constexpr const char* kFileKindGerber = "gerber";
constexpr const char* kFileKindDrill = "drill";


/// Stringify a FILE_KIND value for JSON output.
const char* fileKindToString( GERBVIEW_PROJECT_FILE::FILE_KIND aKind )
{
    return aKind == GERBVIEW_PROJECT_FILE::FILE_KIND::Drill ? kFileKindDrill : kFileKindGerber;
}


/// Parse a FILE_KIND value from JSON; defaults to Gerber for unknown values.
GERBVIEW_PROJECT_FILE::FILE_KIND fileKindFromString( const std::string& aValue )
{
    if( aValue == kFileKindDrill )
        return GERBVIEW_PROJECT_FILE::FILE_KIND::Drill;

    return GERBVIEW_PROJECT_FILE::FILE_KIND::Gerber;
}


/// Resolve @p aPath against @p aBaseDir.  Absolute paths are returned as-is.
wxString resolveAgainst( const wxString& aPath, const wxString& aBaseDir )
{
    wxFileName fn( aPath );

    if( fn.IsAbsolute() )
        return fn.GetFullPath();

    if( aBaseDir.IsEmpty() )
        return fn.GetFullPath();

    fn.MakeAbsolute( aBaseDir );
    return fn.GetFullPath();
}


/// Convert an absolute @p aFullPath to a path relative to @p aBaseDir when it
/// makes sense; otherwise return the absolute path unchanged.
wxString relativizeAgainst( const wxString& aFullPath, const wxString& aBaseDir )
{
    if( aFullPath.IsEmpty() || aBaseDir.IsEmpty() )
        return aFullPath;

    wxFileName fn( aFullPath );

    if( !fn.IsAbsolute() )
        return aFullPath;

    wxFileName relFn = fn;
    relFn.MakeRelativeTo( aBaseDir );

    // Refuse to produce a wildly distant relative path -- a couple of "..":s
    // is fine for sibling folders, but anything beyond that is usually worse
    // for the user than just storing the absolute path.
    int upCount = 0;

    for( const wxString& seg : relFn.GetDirs() )
    {
        if( seg == wxT( ".." ) )
            ++upCount;
        else
            break;
    }

    if( upCount > 4 )
        return fn.GetFullPath();

    return relFn.GetFullPath();
}

} // namespace


void GERBVIEW_PROJECT_FILE::Reset()
{
    m_Files.clear();
    m_Colors.clear();
    m_VisibleInnerCount = 2;
    m_ColorSchemeName.clear();
}


void GERBVIEW_PROJECT_FILE::CaptureFromFrame( GERBVIEW_FRAME* aFrame )
{
    Reset();

    if( !aFrame )
        return;

    GERBER_FILE_IMAGE_LIST* images = aFrame->GetImagesList();

    if( !images )
        return;

    for( unsigned slot = 0; slot < images->ImagesMaxCount(); ++slot )
    {
        GERBER_FILE_IMAGE* img = images->GetGbrImage( static_cast<int>( slot ) );

        if( !img )
            continue;

        FILE_ENTRY entry;
        entry.path = img->m_FileName;
        entry.kind = ( dynamic_cast<EXCELLON_IMAGE*>( img ) != nullptr ) ? FILE_KIND::Drill : FILE_KIND::Gerber;

        GERBVIEW_LAYER_ROLE role = LookupRoleForSlot( aFrame, static_cast<int>( slot ) );

        if( role != GERBVIEW_LAYER_ROLE::Unassigned )
            entry.roleKey = RoleSerializedKey( role );

        entry.visible = aFrame->IsLayerVisible( static_cast<int>( slot ) );

        m_Files.push_back( std::move( entry ) );
    }

    if( GERBVIEW_SETTINGS* cfg = aFrame->gvconfig() )
    {
        m_Colors = cfg->m_LayerRoles.colors;
        m_VisibleInnerCount = cfg->m_LayerRoles.visibleInnerCount;
        m_ColorSchemeName = cfg->m_LayerRoles.loadedSchemeName;
    }
}


bool GERBVIEW_PROJECT_FILE::ApplyToFrame( GERBVIEW_FRAME* aFrame, const wxString& aProjectDir, REPORTER* aReporter )
{
    if( !aFrame )
        return false;

    aFrame->Clear_DrawLayers( false );

    // 1) Push palette + settings into GERBVIEW_SETTINGS BEFORE loading files so
    //    that the per-slot role-apply done during load picks up the new colors.
    if( GERBVIEW_SETTINGS* cfg = aFrame->gvconfig() )
    {
        cfg->m_LayerRoles.colors = m_Colors;
        cfg->m_LayerRoles.visibleInnerCount = m_VisibleInnerCount;
        cfg->m_LayerRoles.loadedSchemeName = m_ColorSchemeName;

        // Pre-register the role assignments for every file we are about to load
        // (keyed by basename, same convention as the layer-roles dialog uses).
        for( const FILE_ENTRY& entry : m_Files )
        {
            if( entry.roleKey.empty() )
                continue;

            GERBVIEW_LAYER_ROLE role = RoleFromSerializedKey( entry.roleKey );

            if( role == GERBVIEW_LAYER_ROLE::Unassigned )
                continue;

            wxString base = wxFileName( entry.path ).GetFullName();

#ifdef __WINDOWS__
            base.MakeLower();
#endif
            cfg->m_LayerRoles.assignments[base] = role;
        }
    }

    // 2) Load the files via the existing pipeline so all the usual side-effects
    //    (file history, image creation, role re-apply) happen.
    wxArrayString    filenames;
    std::vector<int> filetypes;

    for( const FILE_ENTRY& entry : m_Files )
    {
        filenames.Add( resolveAgainst( entry.path, aProjectDir ) );
        filetypes.push_back( entry.kind == FILE_KIND::Drill ? 1 : 0 );
    }

    bool ok = true;

    if( !filenames.IsEmpty() )
        ok = aFrame->LoadListOfGerberAndDrillFiles( aProjectDir, filenames, &filetypes );

    // 3) Re-apply roles + colors so newly created slots get their assigned color.
    ReapplyAllLayerRoles( aFrame );

    // 4) Restore per-layer visibility (best-effort; only files that loaded land here).
    GERBER_FILE_IMAGE_LIST* images = aFrame->GetImagesList();

    if( images )
    {
        LSET vis = aFrame->GetVisibleLayers();

        for( unsigned slot = 0; slot < images->ImagesMaxCount(); ++slot )
        {
            GERBER_FILE_IMAGE* img = images->GetGbrImage( static_cast<int>( slot ) );

            if( !img )
                continue;

            wxString base = wxFileName( img->m_FileName ).GetFullName();

            for( const FILE_ENTRY& entry : m_Files )
            {
                if( wxFileName( entry.path ).GetFullName().IsSameAs( base, false ) )
                {
                    vis.set( static_cast<int>( slot ), entry.visible );
                    break;
                }
            }
        }

        aFrame->SetVisibleLayers( vis );
    }

    if( !ok && aReporter )
        aReporter->Report( _( "Some project files could not be loaded." ), RPT_SEVERITY_WARNING );

    return ok;
}


bool GERBVIEW_PROJECT_FILE::LoadFromFile( const wxString& aFullFileName, REPORTER* aReporter )
{
    Reset();

    nlohmann::json js;

    try
    {
        std::ifstream in( aFullFileName.ToStdString() );

        if( !in.is_open() )
        {
            if( aReporter )
                aReporter->Report( wxString::Format( _( "Could not open '%s'." ), aFullFileName ), RPT_SEVERITY_ERROR );
            return false;
        }

        in >> js;
    }
    catch( const std::exception& e )
    {
        if( aReporter )
            aReporter->Report( wxString::Format( _( "Failed to parse project: %s" ), wxString::FromUTF8( e.what() ) ),
                               RPT_SEVERITY_ERROR );
        return false;
    }

    if( !js.is_object() )
    {
        if( aReporter )
            aReporter->Report( _( "Project file root must be a JSON object." ), RPT_SEVERITY_ERROR );
        return false;
    }

    // Mildly defensive: warn but accept files that don't carry the kind tag.
    if( js.contains( kKeyKind ) && js[kKeyKind].is_string() )
    {
        if( js[kKeyKind].get<std::string>() != PROJECT_KIND && aReporter )
        {
            aReporter->Report( _( "Project file 'kind' field is not 'kicad_gerbview_project'." ),
                               RPT_SEVERITY_WARNING );
        }
    }

    // files[]
    if( js.contains( kKeyFiles ) && js[kKeyFiles].is_array() )
    {
        for( const nlohmann::json& entry : js[kKeyFiles] )
        {
            if( !entry.is_object() || !entry.contains( kKeyFilePath ) )
                continue;

            FILE_ENTRY fe;
            fe.path = wxString::FromUTF8( entry[kKeyFilePath].get<std::string>().c_str() );

            if( entry.contains( kKeyFileKind ) && entry[kKeyFileKind].is_string() )
                fe.kind = fileKindFromString( entry[kKeyFileKind].get<std::string>() );

            if( entry.contains( kKeyFileRole ) && entry[kKeyFileRole].is_string() )
                fe.roleKey = entry[kKeyFileRole].get<std::string>();

            if( entry.contains( kKeyFileVisible ) && entry[kKeyFileVisible].is_boolean() )
                fe.visible = entry[kKeyFileVisible].get<bool>();

            m_Files.push_back( std::move( fe ) );
        }
    }

    // colors{}
    if( js.contains( kKeyColors ) && js[kKeyColors].is_object() )
    {
        for( auto it = js[kKeyColors].begin(); it != js[kKeyColors].end(); ++it )
        {
            if( !it.value().is_string() )
                continue;

            KIGFX::COLOR4D c;

            if( !c.SetFromHexString( wxString::FromUTF8( it.value().get<std::string>() ) ) )
                continue;

            std::string key = it.key();

            // Migration: legacy "inner_copper" -> "inner_copper_1".
            if( key == "inner_copper" )
                key = "inner_copper_1";

            m_Colors[key] = c;
        }
    }

    // settings{}
    if( js.contains( kKeySettings ) && js[kKeySettings].is_object() )
    {
        const nlohmann::json& s = js[kKeySettings];

        if( s.contains( kKeyVisibleInnerCount ) && s[kKeyVisibleInnerCount].is_number_integer() )
        {
            int n = s[kKeyVisibleInnerCount].get<int>();
            m_VisibleInnerCount = std::clamp( n, 2, MAX_INNER_COPPER_LAYERS );
        }

        if( s.contains( kKeyColorSchemeName ) && s[kKeyColorSchemeName].is_string() )
        {
            m_ColorSchemeName = wxString::FromUTF8( s[kKeyColorSchemeName].get<std::string>().c_str() );
        }
    }

    return true;
}


bool GERBVIEW_PROJECT_FILE::SaveToFile( const wxString& aFullFileName, REPORTER* aReporter )
{
    nlohmann::json js;
    js[kKeySchemaVersion] = CURRENT_SCHEMA_VERSION;
    js[kKeyKind] = PROJECT_KIND;
    js[kKeyGenerator] = "GerbView";

    // Project directory used to make file paths relative on save.
    wxString projectDir = wxFileName( aFullFileName ).GetPath();

    nlohmann::json filesArr = nlohmann::json::array();

    for( const FILE_ENTRY& entry : m_Files )
    {
        nlohmann::json je = nlohmann::json::object();
        wxString       path = relativizeAgainst( entry.path, projectDir );

        je[kKeyFilePath] = std::string( path.ToUTF8() );
        je[kKeyFileKind] = fileKindToString( entry.kind );

        if( !entry.roleKey.empty() )
            je[kKeyFileRole] = entry.roleKey;

        je[kKeyFileVisible] = entry.visible;
        filesArr.push_back( std::move( je ) );
    }

    js[kKeyFiles] = std::move( filesArr );

    nlohmann::json colorsObj = nlohmann::json::object();

    for( const auto& [key, color] : m_Colors )
        colorsObj[key] = color.ToHexString().ToUTF8().data();

    js[kKeyColors] = std::move( colorsObj );

    nlohmann::json settingsObj = nlohmann::json::object();
    settingsObj[kKeyVisibleInnerCount] = m_VisibleInnerCount;
    settingsObj[kKeyColorSchemeName] = std::string( m_ColorSchemeName.ToUTF8() );
    js[kKeySettings] = std::move( settingsObj );

    try
    {
        std::ofstream out( aFullFileName.ToStdString() );

        if( !out.is_open() )
        {
            if( aReporter )
                aReporter->Report( wxString::Format( _( "Could not open '%s' for writing." ), aFullFileName ),
                                   RPT_SEVERITY_ERROR );
            return false;
        }

        out << js.dump( 2 );
    }
    catch( const std::exception& e )
    {
        if( aReporter )
            aReporter->Report( wxString::Format( _( "Failed to write project: %s" ), wxString::FromUTF8( e.what() ) ),
                               RPT_SEVERITY_ERROR );
        return false;
    }

    return true;
}
