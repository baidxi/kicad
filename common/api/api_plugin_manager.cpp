/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2024 Jon Evans <jon@craftyjon.com>
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <fstream>

#include <algorithm>

#include <env_vars.h>
#include <fmt/format.h>
#include <wx/dir.h>
#include <wx/log.h>
#include <wx/process.h>
#include <wx/timer.h>
#include <wx/utils.h>

#include <api/api_plugin_manager.h>
#include <api/api_server.h>
#include <api/api_utils.h>
#include <gestfich.h>
#include <paths.h>
#include <wx_filename.h>
#include <pgm_base.h>
#include <api/python_manager.h>
#include <reporter.h>
#include <settings/settings_manager.h>
#include <settings/common_settings.h>


wxDEFINE_EVENT( EDA_EVT_PLUGIN_MANAGER_JOB_FINISHED, wxCommandEvent );
wxDEFINE_EVENT( EDA_EVT_PLUGIN_AVAILABILITY_CHANGED, wxCommandEvent );


class ACTION_PROCESS : public wxProcess
{
public:
    ACTION_PROCESS( std::function<void( int, const wxString&, const wxString& )> aCallback ) :
            wxProcess(),
            m_callback( std::move( aCallback ) )
    {}

    void OnTerminate( int aPid, int aStatus ) override
    {
        if( m_callback )
        {
            wxString output, error;

            if( wxInputStream* processOut = GetInputStream() )
            {
                while( processOut->CanRead() )
                {
                    char buffer[4096];
                    buffer[ processOut->Read( buffer, sizeof( buffer ) - 1 ).LastRead() ] = '\0';
                    output.append( buffer, processOut->LastRead() );
                }
            }

            if( wxInputStream* processErr = GetErrorStream() )
            {
                while( processErr->CanRead() )
                {
                    char buffer[4096];
                    buffer[ processErr->Read( buffer, sizeof( buffer ) - 1 ).LastRead() ] = '\0';
                    error.append( buffer, processErr->LastRead() );
                }
            }

            m_callback( aStatus, output, error );
        }

        wxProcess::OnTerminate( aPid, aStatus );
    }

private:
    std::function<void( int, const wxString&, const wxString& )> m_callback;
};


#ifdef _WIN32
/**
 * Prepends the given interpreter's directory to the child process PATH.  The venv module
 * spawns <venv>\Scripts\python.exe internally to run ensurepip, and that launcher may end
 * up resolving the VC runtime DLLs (vcruntime140.dll) through PATH when they are not
 * installed system-wide.  Without the interpreter's directory on PATH such launches fail
 * with a missing-VCRUNTIME140.dll error.
 */
static void prependInterpreterDirToPath( wxExecuteEnv& aEnv, const wxString& aInterpreter )
{
    wxFileName dir( aInterpreter, wxEmptyString );

    wxString path;

    if( aEnv.env.contains( wxS( "PATH" ) ) )
        path = aEnv.env.at( wxS( "PATH" ) );

    aEnv.env[wxS( "PATH" )] = dir.GetPath() + wxS( ";" ) + path;
}
#endif


#ifdef _WIN32
/**
 * Copies the VC runtime DLLs from the bundled interpreter's directory next to the
 * given venv interpreter.  The application directory is always the first entry of
 * the loader search path, so this guarantees the venv interpreters (and anything
 * they spawn) can load the CRT even when no VC++ redistributable is installed
 * system-wide and no PATH based resolution succeeds.
 */
static void copyVcRuntimeToVenv( const wxString& aVenvInterpreter,
                                 const wxString& aBaseInterpreter )
{
    wxFileName venvDir( aVenvInterpreter, wxEmptyString );
    wxFileName baseDir( aBaseInterpreter, wxEmptyString );

    for( const wxString& dll :
         { wxString( "vcruntime140.dll" ), wxString( "vcruntime140_1.dll" ),
           wxString( "msvcp140.dll" ) } )
    {
        wxFileName src( baseDir.GetPath(), dll );
        wxFileName dst( venvDir.GetPath(), dll );

        if( src.FileExists() && !dst.FileExists() )
            wxCopyFile( src.GetFullPath(), dst.GetFullPath() );
    }
}
#endif


static void reportPluginActionMessage( REPORTER* aReporter, const wxString& aActionName,
                                       const wxString& aMessage )
{
    if( !aReporter || aMessage.IsEmpty() )
        return;

    KI_ERROR error( RPT_SEVERITY_ERROR );
    error.SetTitle( wxString::Format( _( "Plugin action '%s'" ), aActionName ) );
    error.SetDescription( aMessage );
    aReporter->Report( error );
}


static void reportPluginLoadMessage( REPORTER* aReporter, const wxString& aPluginName,
                                     const wxString& aMessage )
{
    if( !aReporter || aMessage.IsEmpty() )
        return;

    aReporter->Report( wxString::Format( _( "Plugin '%s': %s" ), aPluginName, aMessage ),
                       RPT_SEVERITY_ERROR );
}


static void reportPluginLoadMessage( REPORTER* aReporter, const wxString& aPluginName,
                                     const wxString& aDescription, const wxString& aDebugText )
{
    if( !aReporter || ( aDescription.IsEmpty() && aDebugText.IsEmpty() ) )
        return;

    KI_ERROR error( RPT_SEVERITY_ERROR );
    error.SetTitle( wxString::Format( _( "Error loading plugin '%s'" ), aPluginName ) );

    if( !aDescription.IsEmpty() )
        error.SetDescription( aDescription );

    if( !aDebugText.IsEmpty() )
        error.SetDebugText( aDebugText );

    aReporter->Report( error );
}


static void reportPluginActionResult( REPORTER* aReporter, const wxString& aActionName,
                                      int aRetVal, const wxString& aError )
{
    wxString trimmedError = aError;
    trimmedError.Trim();
    trimmedError.Trim( false );

    if( aRetVal != 0 )
    {
        KI_ERROR error( RPT_SEVERITY_ERROR );
        error.SetTitle( wxString::Format( _( "Plugin action '%s'" ), aActionName ) );
        error.SetDescription( wxString::Format( _( "exited with code %d" ), aRetVal ) );

        if( !trimmedError.IsEmpty() )
            error.SetDebugText( trimmedError );

        if( aReporter )
            aReporter->Report( error );
    }
    else if( !trimmedError.IsEmpty() )
    {
        reportPluginActionMessage( aReporter, aActionName, trimmedError );
    }
}


API_PLUGIN_MANAGER::API_PLUGIN_MANAGER( wxEvtHandler* aEvtHandler ) :
        wxEvtHandler(),
        m_parent( aEvtHandler ),
        m_lastPid( 0 ),
        m_raiseTimer( nullptr )
{
    // Read and store pcm schema
    wxFileName schemaFile( PATHS::GetStockDataPath( true ), wxS( "api.v1.schema.json" ) );
    schemaFile.Normalize( FN_NORMALIZE_FLAGS | wxPATH_NORM_ENV_VARS );
    schemaFile.AppendDir( wxS( "schemas" ) );

    m_schema_validator = std::make_unique<JSON_SCHEMA_VALIDATOR>( schemaFile );

    Bind( EDA_EVT_PLUGIN_MANAGER_JOB_FINISHED, &API_PLUGIN_MANAGER::processNextJob, this );
}


class PLUGIN_TRAVERSER : public wxDirTraverser
{
private:
    std::function<void( const wxFileName& )> m_action;

public:
    explicit PLUGIN_TRAVERSER( std::function<void( const wxFileName& )> aAction )
            : m_action( std::move( aAction ) )
    {
    }

    wxDirTraverseResult OnFile( const wxString& aFilePath ) override
    {
        wxFileName file( aFilePath );

        if( file.GetFullName() == wxS( "plugin.json" ) )
            m_action( file );

        return wxDIR_CONTINUE;
    }

    wxDirTraverseResult OnDir( const wxString& dirPath ) override
    {
        return wxDIR_CONTINUE;
    }
};


void API_PLUGIN_MANAGER::ReloadPlugins( std::optional<wxString> aDirectoryToScan,
                                        std::shared_ptr<REPORTER> aReporter )
{
    m_reloadReporter = std::move( aReporter );

    m_plugins.clear();
    m_pluginsCache.clear();
    m_actionsCache.clear();
    m_environmentCache.clear();
    m_buttonBindings.clear();
    m_menuBindings.clear();
    m_readyPlugins.clear();

    PLUGIN_TRAVERSER loader(
            [&]( const wxFileName& aFile )
            {
                wxLogTrace( traceApi, wxString::Format( "Manager: loading plugin from %s",
                                                        aFile.GetFullPath() ) );

                auto plugin = std::make_unique<API_PLUGIN>( aFile, *m_schema_validator );

                if( plugin->IsOk() )
                {
                    const wxString& id = plugin->Identifier();

                    if( m_pluginsCache.contains( id ) )
                    {
                        wxLogTrace( traceApi, wxString::Format( "Manager: identifier %s already present, reloading", id ) );

                        for( const PLUGIN_ACTION& action : m_pluginsCache[id]->Actions() )
                            m_actionsCache[action.identifier] = &action;

                        m_pluginsCache.erase( id );
                        return;
                    }

                    m_pluginsCache[id] = plugin.get();

                    for( const PLUGIN_ACTION& action : plugin->Actions() )
                        m_actionsCache[action.identifier] = &action;

                    m_plugins.insert( std::move( plugin ) );
                }
                else
                {
                    wxLogTrace( traceApi, "Manager: loading failed" );

                    reportPluginLoadMessage( m_reloadReporter.get(), aFile.GetName(),
                                             plugin->ErrorMessage() );
                }
            } );

    if( aDirectoryToScan )
    {
        wxDir customDir( *aDirectoryToScan );
        wxLogTrace( traceApi, wxString::Format( "Manager: scanning custom path (%s) for plugins...",
                                                customDir.GetName() ) );
        customDir.Traverse( loader );
    }
    else
    {
        wxDir systemPluginsDir( PATHS::GetStockPluginsPath() );

        if( systemPluginsDir.IsOpened() )
        {
            wxLogTrace( traceApi, wxString::Format( "Manager: scanning system path (%s) for plugins...",
                                                    systemPluginsDir.GetName() ) );
            systemPluginsDir.Traverse( loader );
        }

        wxString thirdPartyPath;
        const ENV_VAR_MAP& env = Pgm().GetLocalEnvVariables();

        if( std::optional<wxString> v = ENV_VAR::GetVersionedEnvVarValue( env, wxT( "3RD_PARTY" ) ) )
            thirdPartyPath = *v;
        else
            thirdPartyPath = PATHS::GetDefault3rdPartyPath();

        wxDir thirdParty( thirdPartyPath );

        if( thirdParty.IsOpened() )
        {
            wxLogTrace( traceApi, wxString::Format( "Manager: scanning PCM path (%s) for plugins...",
                                                    thirdParty.GetName() ) );
            thirdParty.Traverse( loader );
        }

        wxDir userPluginsDir( PATHS::GetUserPluginsPath() );

        if( userPluginsDir.IsOpened() )
        {
            wxLogTrace( traceApi, wxString::Format( "Manager: scanning user path (%s) for plugins...",
                                                    userPluginsDir.GetName() ) );
            userPluginsDir.Traverse( loader );
        }
    }

    processPluginDependencies();

    if( !Busy() )
        m_reloadReporter.reset();

    wxCommandEvent* evt = new wxCommandEvent( EDA_EVT_PLUGIN_AVAILABILITY_CHANGED, wxID_ANY );
    m_parent->QueueEvent( evt );
}


std::map<int, wxString>& API_PLUGIN_MANAGER::ButtonBindings( FRAME_T aFrame )
{
    return m_buttonBindings[aFrame];
}


std::map<int, wxString>& API_PLUGIN_MANAGER::MenuBindings( FRAME_T aFrame )
{
    return m_menuBindings[aFrame];
}


bool API_PLUGIN_MANAGER::RecreatePluginEnvironment( const wxString& aIdentifier )
{
    if( !m_pluginsCache.contains( aIdentifier ) )
        return false;

    const API_PLUGIN* plugin = m_pluginsCache.at( aIdentifier );
    wxCHECK( plugin, false );

    if( plugin->Runtime().type != PLUGIN_RUNTIME_TYPE::PYTHON )
        return false;

    std::optional<wxString> env = PYTHON_MANAGER::GetPythonEnvironment( plugin->Identifier() );
    wxCHECK( env.has_value(), false );

    wxFileName envConfigPath( *env, wxS( "pyvenv.cfg" ) );
    envConfigPath.MakeAbsolute();

    if( envConfigPath.DirExists() && envConfigPath.Rmdir( wxPATH_RMDIR_RECURSIVE ) )
    {
        wxLogTrace( traceApi,
                    wxString::Format( "Manager: Removed existing Python environment at %s for %s",
                                      envConfigPath.GetPath(), plugin->Identifier() ) );

        JOB job;
        job.type = JOB_TYPE::CREATE_ENV;
        job.identifier = plugin->Identifier();
        job.plugin_path = plugin->BasePath();
        job.env_path = envConfigPath.GetPath();
        m_jobs.emplace_back( job );

        // Caller should send EDA_EVT_PLUGIN_MANAGER_JOB_FINISHED
        return true;
    }

    wxLogTrace( traceApi,
                wxString::Format( "Manager: could not remove Python environment at %s for %s",
                                  envConfigPath.GetPath(), plugin->Identifier() ) );

    return false;
}


std::optional<const PLUGIN_ACTION*> API_PLUGIN_MANAGER::GetAction( const wxString& aIdentifier )
{
    if( !m_actionsCache.contains( aIdentifier ) )
        return std::nullopt;

    return m_actionsCache.at( aIdentifier );
}


void API_PLUGIN_MANAGER::recreateCancelledPlugin( const wxString& aIdentifier )
{
    m_cancelledPlugins.erase( aIdentifier );
    m_busyPlugins.erase( aIdentifier );

    if( !RecreatePluginEnvironment( aIdentifier ) )
        return;

    m_busyPlugins.insert( aIdentifier );

    wxCommandEvent* evt = new wxCommandEvent( EDA_EVT_PLUGIN_MANAGER_JOB_FINISHED, wxID_ANY );
    QueueEvent( evt );
}


void API_PLUGIN_MANAGER::HandlePythonInterpreterChanged()
{
    bool addedAnyJobs = false;

    for( const std::unique_ptr<API_PLUGIN>& plugin : m_plugins )
    {
        if( plugin->Runtime().type != PLUGIN_RUNTIME_TYPE::PYTHON )
            continue;

        std::optional<wxString> env = PYTHON_MANAGER::GetPythonEnvironment( plugin->Identifier() );

        if( !env )
            continue;

        if( !PYTHON_MANAGER::IsVenvStale( *env, Pgm().GetCommonSettings()->m_Api.python_interpreter ) )
            continue;

        m_readyPlugins.erase( plugin->Identifier() );

        if( m_busyPlugins.contains( plugin->Identifier() ) )
        {
            wxLogTrace( traceApi, wxString::Format( "Manager: cancelling in-flight job for %s after interpreter change",
                                                    plugin->Identifier() ) );

            if( !cancelPluginJobs( plugin->Identifier() ) )
            {
                // Nothing was actually running: no termination callback will fire, so schedule the recreation directly.
                recreateCancelledPlugin( plugin->Identifier() );
                addedAnyJobs = true;
            }
        }
        else
        {
            wxLogTrace( traceApi, wxString::Format( "Manager: recreating Python env for %s after interpreter change",
                                                    plugin->Identifier() ) );

            m_busyPlugins.insert( plugin->Identifier() );

            if( RecreatePluginEnvironment( plugin->Identifier() ) )
                addedAnyJobs = true;
            else
                m_busyPlugins.erase( plugin->Identifier() );
        }
    }

    if( addedAnyJobs )
    {
        wxCommandEvent* evt = new wxCommandEvent( EDA_EVT_PLUGIN_MANAGER_JOB_FINISHED, wxID_ANY );
        QueueEvent( evt );
    }
}


int API_PLUGIN_MANAGER::doInvokeAction( const wxString& aIdentifier,
                                        std::vector<wxString> aExtraArgs,
                                        bool aSync, wxString* aStdout, wxString* aStderr,
                                        std::shared_ptr<REPORTER> aReporter )
{
    if( !m_actionsCache.contains( aIdentifier ) )
    {
        reportPluginActionMessage( aReporter.get(), aIdentifier, _( "action is not registered" ) );
        return -1;
    }

    const PLUGIN_ACTION* action = m_actionsCache.at( aIdentifier );
    const API_PLUGIN& plugin = action->plugin;

    if( !m_readyPlugins.count( plugin.Identifier() ) )
    {
        wxLogTrace( traceApi, wxString::Format( "Manager: Plugin %s is not ready",
                                                plugin.Identifier() ) );
        return -1;
    }

    wxFileName pluginFile( action->entrypoint );
    pluginFile.MakeAbsolute( plugin.BasePath() );
    pluginFile.Normalize( wxPATH_NORM_ABSOLUTE | wxPATH_NORM_SHORTCUT | wxPATH_NORM_DOTS
                          | wxPATH_NORM_TILDE, plugin.BasePath() );
    wxString pluginPath = pluginFile.GetFullPath();

    std::vector<const wchar_t*> args;
    std::optional<wxString> py;

    switch( plugin.Runtime().type )
    {
    case PLUGIN_RUNTIME_TYPE::PYTHON:
    {
        py = PYTHON_MANAGER::GetVirtualPython( plugin.Identifier() );

        if( !py )
        {
            wxLogTrace( traceApi, wxString::Format( "Manager: Python interpreter for %s not found",
                                                    plugin.Identifier() ) );
            reportPluginActionMessage( aReporter.get(), action->name,
                                       _( "missing plugin environment" ) );
            return -1;
        }

        if( !pluginFile.IsFileReadable() )
        {
            wxLogTrace( traceApi, wxString::Format( "Manager: Python entrypoint %s is not readable",
                                                    pluginFile.GetFullPath() ) );
            reportPluginActionMessage( aReporter.get(), action->name,
                                       wxString::Format( _( "entrypoint '%s' could not be read" ),
                                                         pluginFile.GetFullPath() ) );
            return -1;
        }

        std::optional<wxString> pythonHome =
                PYTHON_MANAGER::GetPythonEnvironment( plugin.Identifier() );

        PYTHON_MANAGER manager( *py );
        wxExecuteEnv   env;
        wxGetEnvMap( &env.env );

        if( Pgm().ApiServerOrNull() )
        {
            env.env[wxS( "KICAD_API_SOCKET" )] = Pgm().GetApiServer().SocketPath();
            env.env[wxS( "KICAD_API_TOKEN" )] = Pgm().GetApiServer().Token();
        }

        env.cwd = pluginFile.GetPath();

#ifdef _WIN32
        wxString systemRoot;
        wxGetEnv( wxS( "SYSTEMROOT" ), &systemRoot );
        env.env[wxS( "SYSTEMROOT" )] = systemRoot;

        // The venv\Scripts\pythonw.exe launcher may resolve the VC runtime DLLs
        // through PATH; make sure the interpreter's directory is there.  Also copy
        // the runtime DLLs next to the venv interpreter as a guaranteed fallback:
        // the application directory always precedes everything else in the loader
        // search path.
        prependInterpreterDirToPath( env, Pgm().GetCommonSettings()->m_Api.python_interpreter );
        copyVcRuntimeToVenv( *py, Pgm().GetCommonSettings()->m_Api.python_interpreter );

        if( Pgm().GetCommonSettings()->m_Api.python_interpreter == FindKicadFile( "pythonw.exe" )
            || PATHS::RunningFromBuildDir() )
        {
            wxLogTrace( traceApi, "Configured Python is the KiCad one; erasing path overrides..." );
            env.env.erase( "PYTHONHOME" );
            env.env.erase( "PYTHONPATH" );
        }
#endif

        if( pythonHome )
            env.env[wxS( "VIRTUAL_ENV" )] = *pythonHome;

        std::vector<wxString> pyArgs( action->args );
        pyArgs.emplace_back( pluginFile.GetFullPath() );
        pyArgs.insert( pyArgs.end(), aExtraArgs.begin(), aExtraArgs.end() );

        if( aSync )
        {
            wxString stdOut;
            wxString stdErr;
            wxString* stdoutSink = aStdout ? aStdout : &stdOut;
            wxString* stderrSink = aStderr ? aStderr : &stdErr;
            int ret = manager.ExecuteSync( pyArgs, stdoutSink, stderrSink, &env );
            reportPluginActionResult( aReporter.get(), action->name, ret, *stderrSink );
            return ret;
        }

        [[maybe_unused]] long pid = manager.Execute( pyArgs,
                [aReporter, actionName = action->name]( int aRetVal, const wxString& aOutput,
                                                        const wxString& aError )
                {
                    wxLogTrace( traceApi,
                                wxString::Format( "Manager: action exited with code %d", aRetVal ) );

                    if( !aError.IsEmpty() )
                        wxLogTrace( traceApi, wxString::Format( "Manager: action stderr: %s", aError ) );

                    reportPluginActionResult( aReporter.get(), actionName, aRetVal, aError );
                },
                &env, true );

        if( !pid )
        {
            reportPluginActionMessage( aReporter.get(), action->name,
                                       _( "process could not be created" ) );
            return -1;
        }

#ifdef __WXMAC__
        if( pid )
        {
            if( !m_raiseTimer )
            {
                m_raiseTimer = new wxTimer( this );

                Bind( wxEVT_TIMER,
                        [&]( wxTimerEvent& )
                        {
                            wxString script = wxString::Format(
                                wxS( "tell application \"System Events\"\n"
                                  "  set plist to every process whose unix id is %ld\n"
                                  "  repeat with proc in plist\n"
                                  "    set the frontmost of proc to true\n"
                                  "  end repeat\n"
                                  "end tell" ), m_lastPid );

                            wxString cmd = wxString::Format( "osascript -e '%s'", script );
                            wxLogTrace( traceApi, wxString::Format( "Execute: %s", cmd ) );
                            wxExecute( cmd );
                        },
                        m_raiseTimer->GetId() );
            }

            m_lastPid = pid;
            m_raiseTimer->StartOnce( 250 );
        }
#endif

        break;
    }

    case PLUGIN_RUNTIME_TYPE::EXEC:
    {
        if( !pluginFile.IsFileExecutable() )
        {
            wxLogTrace( traceApi, wxString::Format( "Manager: Exec entrypoint %s is not executable",
                                                    pluginFile.GetFullPath() ) );
            reportPluginActionMessage( aReporter.get(), action->name,
                                       wxString::Format( _( "entrypoint '%s' is not executable" ),
                                                         pluginFile.GetFullPath() ) );
            return -1;
        }

        wxExecuteEnv env;
        wxGetEnvMap( &env.env );

        if( Pgm().ApiServerOrNull() )
        {
            env.env[wxS( "KICAD_API_SOCKET" )] = Pgm().GetApiServer().SocketPath();
            env.env[wxS( "KICAD_API_TOKEN" )] = Pgm().GetApiServer().Token();
        }

        env.cwd = pluginFile.GetPath();

        long pidOrRetCode = 0;

        if( aSync )
        {
            wxString cmd = pluginPath;

            for( const wxString& arg : action->args )
                cmd << " " << arg;

            for( const wxString& arg : aExtraArgs )
                cmd << " " << arg;

            wxArrayString out, err;

            pidOrRetCode = wxExecute( cmd, out, err, wxEXEC_BLOCK, &env );

            if( aStdout )
            {
                for( const wxString& line : out )
                    *aStdout << line << "\n";
            }

            wxString stdErr;

            for( const wxString& line : err )
                stdErr << line << "\n";

            if( aStderr )
                *aStderr = stdErr;

            reportPluginActionResult( aReporter.get(), action->name, pidOrRetCode, stdErr );
            return pidOrRetCode;
        }
        else
        {
            ACTION_PROCESS* process = new ACTION_PROCESS(
                    [aReporter, actionName = action->name]( int aRetVal, const wxString& aOutput,
                                                            const wxString& aError )
                    {
                        wxLogTrace( traceApi,
                                    wxString::Format( "Manager: action exited with code %d", aRetVal ) );

                        if( !aError.IsEmpty() )
                            wxLogTrace( traceApi,
                                        wxString::Format( "Manager: action stderr: %s", aError ) );

                        reportPluginActionResult( aReporter.get(), actionName, aRetVal, aError );
                    } );

            process->Redirect();
            args.emplace_back( pluginPath.wc_str() );

            for( const wxString& arg : action->args )
                args.emplace_back( arg.wc_str() );

            for( const wxString& arg : aExtraArgs )
                args.emplace_back( arg.wc_str() );

            args.emplace_back( nullptr );

            pidOrRetCode = wxExecute( const_cast<wchar_t**>( args.data() ),
                                      wxEXEC_ASYNC | wxEXEC_HIDE_CONSOLE, process, &env );

            if( !pidOrRetCode )
                delete process;
        }

        if( !pidOrRetCode )
        {
            wxLogTrace( traceApi, wxString::Format( "Manager: launching action %s failed",
                                                    action->identifier ) );
            reportPluginActionMessage( aReporter.get(), action->name, _( "could not launch plugin" ) );
        }
        else
        {
            wxLogTrace( traceApi, wxString::Format( "Manager: launching action %s -> pid %ld",
                                                    action->identifier, pidOrRetCode ) );
        }
        break;
    }

    default:
        wxLogTrace( traceApi, wxString::Format( "Manager: unhandled runtime for action %s",
                                                action->identifier ) );
    }

    return -1;
}


void API_PLUGIN_MANAGER::InvokeAction( const wxString& aIdentifier,
                                       std::shared_ptr<REPORTER> aReporter )
{
    doInvokeAction( aIdentifier, {}, false, nullptr, nullptr, std::move( aReporter ) );
}


int API_PLUGIN_MANAGER::InvokeActionSync( const wxString& aIdentifier, std::vector<wxString> aExtraArgs,
                                          wxString* aStdout, wxString* aStderr,
                                          std::shared_ptr<REPORTER> aReporter )
{
    return doInvokeAction( aIdentifier, aExtraArgs, true, aStdout, aStderr,
                           std::move( aReporter ) );
}


std::vector<const PLUGIN_ACTION*> API_PLUGIN_MANAGER::GetActionsForScope( PLUGIN_ACTION_SCOPE aScope )
{
    std::vector<const PLUGIN_ACTION*> actions;

    for( auto& [identifier, action] : m_actionsCache )
    {
        if( !m_readyPlugins.count( action->plugin.Identifier() ) )
            continue;

        if( action->scopes.count( aScope ) )
            actions.emplace_back( action );
    }

    return actions;
}


wxString API_PLUGIN_MANAGER::pluginName( const wxString& aIdentifier ) const
{
    if( m_pluginsCache.contains( aIdentifier ) )
        return m_pluginsCache.at( aIdentifier )->Name();

    return aIdentifier;
}


bool API_PLUGIN_MANAGER::cancelPluginJobs( const wxString& aIdentifier )
{
    std::erase_if( m_jobs,
                   [&aIdentifier]( const JOB& aJob )
                   {
                       return aJob.identifier == aIdentifier;
                   } );

    long pid = m_runningPids[aIdentifier];
    m_runningPids.erase( aIdentifier );

    if( pid != 0 )
        wxKill( pid, wxSIGTERM, nullptr, wxKILL_CHILDREN );

    m_cancelledPlugins.insert( aIdentifier );

    return pid != 0;
}


void API_PLUGIN_MANAGER::processPluginDependencies()
{
    bool addedAnyJobs = false;

    for( const std::unique_ptr<API_PLUGIN>& plugin : m_plugins )
    {
        if( m_busyPlugins.contains( plugin->Identifier() ) )
            continue;

        wxLogTrace( traceApi, wxString::Format( "Manager: processing dependencies for %s",
                                                plugin->Identifier() ) );
        m_environmentCache[plugin->Identifier()] = wxEmptyString;

        if( plugin->Runtime().type != PLUGIN_RUNTIME_TYPE::PYTHON )
        {
            wxLogTrace( traceApi, wxString::Format( "Manager: %s is not a Python plugin, all set",
                                                    plugin->Identifier() ) );
            m_readyPlugins.insert( plugin->Identifier() );
            continue;
        }

        std::optional<wxString> env = PYTHON_MANAGER::GetPythonEnvironment( plugin->Identifier() );

        if( !env )
        {
            wxLogTrace( traceApi, wxString::Format( "Manager: could not create env for %s",
                                                     plugin->Identifier() ) );
            continue;
        }

        m_busyPlugins.insert( plugin->Identifier() );

        wxFileName envConfigPath( *env, wxS( "pyvenv.cfg" ) );
        envConfigPath.MakeAbsolute();

        if( envConfigPath.IsFileReadable() )
        {
            bool broken = !PYTHON_MANAGER::IsVenvUsable( *env );
            bool stale = PYTHON_MANAGER::IsVenvStale( *env, Pgm().GetCommonSettings()->m_Api.python_interpreter );

            if( broken || stale )
            {
                wxLogTrace( traceApi, wxString::Format( "Manager: Python env for %s at %s needs recreation (%s)",
                                                        plugin->Identifier(), envConfigPath.GetPath(),
                                                        broken ? "broken" : "interpreter changed" ) );

                if( RecreatePluginEnvironment( plugin->Identifier() ) )
                {
                    addedAnyJobs = true;
                    continue;
                }

                wxLogTrace( traceApi, wxString::Format( "Manager: could not remove env for %s; "
                                                        "scheduling creation anyway",
                                                        plugin->Identifier() ) );
            }
            else
            {
                wxLogTrace( traceApi, wxString::Format( "Manager: Python env for %s exists at %s", plugin->Identifier(),
                                                        envConfigPath.GetPath() ) );

                JOB job;
                job.type = JOB_TYPE::INSTALL_REQUIREMENTS;
                job.identifier = plugin->Identifier();
                job.plugin_path = plugin->BasePath();
                job.env_path = envConfigPath.GetPath();
                m_jobs.emplace_back( job );
                addedAnyJobs = true;
                continue;
            }
        }

        wxLogTrace( traceApi, wxString::Format( "Manager: will create Python env for %s at %s",
                                                plugin->Identifier(), envConfigPath.GetPath() ) );
        JOB job;
        job.type = JOB_TYPE::CREATE_ENV;
        job.identifier = plugin->Identifier();
        job.plugin_path = plugin->BasePath();
        job.env_path = envConfigPath.GetPath();
        m_jobs.emplace_back( job );
        addedAnyJobs = true;
    }

    if( addedAnyJobs )
    {
        wxCommandEvent* evt = new wxCommandEvent( EDA_EVT_PLUGIN_MANAGER_JOB_FINISHED, wxID_ANY );
        QueueEvent( evt );
    }
}


void API_PLUGIN_MANAGER::processNextJob( wxCommandEvent& aEvent )
{
    if( m_jobs.empty() )
    {
        wxLogTrace( traceApi, "Manager: no more jobs to process" );
        return;
    }

    wxLogTrace( traceApi, wxString::Format( "Manager: begin processing; %zu jobs left in queue",
                                            m_jobs.size() ) );

    JOB& job = m_jobs.front();

    if( job.type == JOB_TYPE::CREATE_ENV )
    {
        wxLogTrace( traceApi, "Manager: Using Python interpreter at %s",
                    Pgm().GetCommonSettings()->m_Api.python_interpreter );
        wxLogTrace( traceApi, wxString::Format( "Manager: creating Python env at %s",
                                                job.env_path ) );

        // Remove any environment directory left behind by a previous failed attempt.  The
        // venv module does not clean up after itself when creation fails partway (e.g. when
        // the ensurepip module is missing), and a partial directory would otherwise keep
        // passing the usability checks on subsequent loads, preventing the environment from
        // ever being properly re-created.
        wxFileName envPath( job.env_path, wxEmptyString );

        if( envPath.DirExists() && envPath.Rmdir( wxPATH_RMDIR_RECURSIVE ) )
        {
            wxLogTrace( traceApi, wxString::Format( "Manager: removed stale environment at %s",
                                                    job.env_path ) );
        }

        wxString interpreter = Pgm().GetCommonSettings()->m_Api.python_interpreter;
        PYTHON_MANAGER manager( interpreter );

        // Inherit the full parent environment: wxExecuteEnv::env REPLACES the child
        // environment, and spawning the interpreter with a bare one (no PATH, TEMP,
        // SYSTEMROOT, ...) breaks the child in various ways on Windows, including
        // spurious vcruntime140.dll loading errors.
        wxExecuteEnv   env;
        wxGetEnvMap( &env.env );

#ifdef _WIN32
        wxString systemRoot;
        wxGetEnv( wxS( "SYSTEMROOT" ), &systemRoot );
        env.env[wxS( "SYSTEMROOT" )] = systemRoot;

        // The venv module spawns <venv>\Scripts\python.exe internally; make sure it can
        // resolve the VC runtime DLLs shipped next to the bundled interpreter.
        prependInterpreterDirToPath( env, interpreter );

        if( Pgm().GetCommonSettings()->m_Api.python_interpreter == FindKicadFile( "pythonw.exe" )
            || PATHS::RunningFromBuildDir() )
        {
            wxLogTrace( traceApi, "Configured Python is the KiCad one; erasing path overrides..." );
            env.env.erase( "PYTHONHOME" );
            env.env.erase( "PYTHONPATH" );
        }
#endif

        std::vector<wxString> args;

        if( PYTHON_MANAGER::EnsurepipAvailable( interpreter ) )
        {
            args = { "-m", "venv", "--system-site-packages", job.env_path };
        }
        else if( wxString venvPyz = PATHS::GetBundledVirtualenvPath();
                 wxFileName( venvPyz ).IsFileReadable() )
        {
            // Some distros ship Python without ensurepip (Debian/Ubuntu without the
            // python3-venv package).  The bundled virtualenv.pyz launcher can create the
            // environment instead; it seeds pip from its own bundled wheels, offline.
            wxLogTrace( traceApi, wxString::Format( "Manager: interpreter is missing ensurepip; "
                                                    "using bundled virtualenv launcher %s",
                                                    venvPyz ) );
            args = { venvPyz, "--system-site-packages", job.env_path };
        }
        else
        {
            // Without ensurepip or the bundled launcher no environment can be created.
            // Fail this job for good instead of leaving the plugin in the busy state.
            wxLogTrace( traceApi, "Manager: interpreter is missing ensurepip and no bundled "
                                  "virtualenv launcher was found" );

            wxString debug = _( "The configured Python interpreter cannot create virtual "
                                "environments because the ensurepip module is not available. "
                                "Install the python3-venv system package, or select a "
                                "different Python interpreter in Preferences." );

            reportPluginLoadMessage( m_reloadReporter.get(), pluginName( job.identifier ),
                                     _( "Could not create plugin environment" ), debug );

            m_busyPlugins.erase( job.identifier );

            wxCommandEvent* evt = new wxCommandEvent( EDA_EVT_PLUGIN_MANAGER_JOB_FINISHED, wxID_ANY );
            QueueEvent( evt );
        }

        if( !args.empty() )
        {
            long pid = manager.Execute( args,
                    [this, job]( int aRetVal, const wxString& aOutput, const wxString& aError )
                    {
                        m_runningPids.erase( job.identifier );

                        wxLogTrace( traceApi,
                                    wxString::Format( "Manager: created venv (python returned %d)", aRetVal ) );

                        if( !aError.IsEmpty() )
                            wxLogTrace( traceApi, wxString::Format( "Manager: venv err: %s", aError ) );

                        if( m_cancelledPlugins.contains( job.identifier ) )
                        {
                            recreateCancelledPlugin( job.identifier );
                            return;
                        }

                        if( aRetVal != 0 )
                        {
                            wxString error = aError;
                            error.Trim().Trim( false );

                            if( error.IsEmpty() )
                                error = wxString::Format( _( "error code %d" ), aRetVal );

                            reportPluginLoadMessage( m_reloadReporter.get(), pluginName( job.identifier ),
                                                     _( "Could not create plugin environment" ), error );

                            // The environment is unusable, so any follow-up jobs queued for this
                            // plugin can only fail as well.  Remove them and clear the busy state,
                            // then delete the partial environment directory so that a later
                            // attempt (e.g. after installing python3-venv) starts from scratch.
                            std::erase_if( m_jobs,
                                           [&job]( const JOB& aJob )
                                           {
                                               return aJob.identifier == job.identifier;
                                           } );
                            m_busyPlugins.erase( job.identifier );

                            wxFileName partialEnv( job.env_path, wxEmptyString );

                            if( partialEnv.DirExists() )
                                partialEnv.Rmdir( wxPATH_RMDIR_RECURSIVE );
                        }

                        wxCommandEvent* evt = new wxCommandEvent( EDA_EVT_PLUGIN_MANAGER_JOB_FINISHED,
                                                                  wxID_ANY );
                        QueueEvent( evt );
                    }, &env );

            m_runningPids[job.identifier] = pid;

            JOB nextJob( job );
            nextJob.type = JOB_TYPE::SETUP_ENV;
            m_jobs.emplace_back( nextJob );
        }
    }
    else if( job.type == JOB_TYPE::SETUP_ENV )
    {
        wxLogTrace( traceApi, wxString::Format( "Manager: setting up environment for %s",
                                                job.plugin_path ) );

        std::optional<wxString> pythonHome = PYTHON_MANAGER::GetPythonEnvironment( job.identifier );
        std::optional<wxString> python = PYTHON_MANAGER::GetVirtualPython( job.identifier );

        if( m_cancelledPlugins.contains( job.identifier ) )
        {
            recreateCancelledPlugin( job.identifier );
        }
        else if( !python )
        {
            wxString debug = wxString::Format( wxS( "Python binary not found at %s" ), job.env_path );
            wxLogTrace( traceApi, wxString::Format( "Manager: error: %s", debug ) );

            reportPluginLoadMessage( m_reloadReporter.get(), pluginName( job.identifier ),
                                     _( "Missing plugin environment" ), debug );

            m_busyPlugins.erase( job.identifier );
            wxCommandEvent* evt = new wxCommandEvent( EDA_EVT_PLUGIN_MANAGER_JOB_FINISHED, wxID_ANY );
            QueueEvent( evt );
        }
        else
        {
            PYTHON_MANAGER manager( *python );

            // Inherit the full parent environment; see the note in the CREATE_ENV branch.
            wxExecuteEnv env;
            wxGetEnvMap( &env.env );

            if( pythonHome )
                env.env[wxS( "VIRTUAL_ENV" )] = *pythonHome;

#ifdef _WIN32
            wxString systemRoot;
            wxGetEnv( wxS( "SYSTEMROOT" ), &systemRoot );
            env.env[wxS( "SYSTEMROOT" )] = systemRoot;

            // The venv\Scripts\python.exe launcher spawned by pip may resolve the VC
            // runtime DLLs through PATH; make sure the interpreter's directory is there.
            prependInterpreterDirToPath( env,
                                         Pgm().GetCommonSettings()->m_Api.python_interpreter );
            copyVcRuntimeToVenv( *python, Pgm().GetCommonSettings()->m_Api.python_interpreter );

            if( Pgm().GetCommonSettings()->m_Api.python_interpreter
                        == FindKicadFile( "pythonw.exe" )
                || PATHS::RunningFromBuildDir() )
            {
                wxLogTrace( traceApi,
                            "Configured Python is the KiCad one; erasing path overrides..." );
                env.env.erase( "PYTHONHOME" );
                env.env.erase( "PYTHONPATH" );
            }
#endif

            std::vector<wxString> args = {
                    "-m",
                    "pip",
                    "install",
                    "--upgrade",
                    "pip"
                };

            long pid = manager.Execute( args,
                [this, job]( int aRetVal, const wxString& aOutput, const wxString& aError )
                {
                    m_runningPids.erase( job.identifier );

                    wxLogTrace( traceApi, wxString::Format( "Manager: upgrade pip returned %d",
                                                            aRetVal ) );

                    if( !aError.IsEmpty() )
                    {
                        wxLogTrace( traceApi,
                                    wxString::Format( "Manager: upgrade pip stderr: %s", aError ) );
                    }

                    if( m_cancelledPlugins.contains( job.identifier ) )
                    {
                        recreateCancelledPlugin( job.identifier );
                        return;
                    }

                    if( aRetVal != 0 )
                    {
                        wxString error = aError;
                        error.Trim().Trim( false );

                        if( error.IsEmpty() )
                            error = wxString::Format( _( "error code %d" ), aRetVal );

                        reportPluginLoadMessage( m_reloadReporter.get(), pluginName( job.identifier ),
                                                 _( "Could not create plugin environment" ), error );
                    }

                    wxCommandEvent* evt =
                            new wxCommandEvent( EDA_EVT_PLUGIN_MANAGER_JOB_FINISHED, wxID_ANY );
                    QueueEvent( evt );
                }, &env );

            m_runningPids[job.identifier] = pid;

            JOB nextJob( job );
            nextJob.type = JOB_TYPE::INSTALL_REQUIREMENTS;
            m_jobs.emplace_back( nextJob );
        }
    }
    else if( job.type == JOB_TYPE::INSTALL_REQUIREMENTS )
    {
        wxLogTrace( traceApi, wxString::Format( "Manager: installing dependencies for %s",
                                                job.plugin_path ) );

        std::optional<wxString> pythonHome = PYTHON_MANAGER::GetPythonEnvironment( job.identifier );
        std::optional<wxString> python = PYTHON_MANAGER::GetVirtualPython( job.identifier );
        wxFileName reqs = wxFileName( job.plugin_path, "requirements.txt" );

        if( m_cancelledPlugins.contains( job.identifier ) )
        {
            recreateCancelledPlugin( job.identifier );
        }
        else if( !python )
        {
            wxString debug = wxString::Format( wxS( "Python binary not found at %s" ), job.env_path );
            wxLogTrace( traceApi, wxString::Format( "Manager: error: %s", debug ) );

            reportPluginLoadMessage( m_reloadReporter.get(), pluginName( job.identifier ),
                                     _( "Missing plugin environment" ), debug );

            m_busyPlugins.erase( job.identifier );
        }
        else if( !reqs.IsFileReadable() )
        {
            wxLogTrace( traceApi,
                        wxString::Format( "Manager: error: requirements.txt not found at %s",
                                          job.plugin_path ) );
            wxString debug = wxString::Format( wxS( "Expected at %s" ), reqs.GetFullPath() );

            reportPluginLoadMessage( m_reloadReporter.get(), pluginName( job.identifier ),
                                     _( "requirements.txt could not be read" ), debug );

            // No requirements to install; the plugin is complete.
            m_readyPlugins.insert( job.identifier );
            m_busyPlugins.erase( job.identifier );

            wxCommandEvent* availabilityEvt = new wxCommandEvent( EDA_EVT_PLUGIN_AVAILABILITY_CHANGED, wxID_ANY );
            wxTheApp->QueueEvent( availabilityEvt );
        }
        else
        {
            wxLogTrace( traceApi, "Manager: Python exe '%s'", *python );

            PYTHON_MANAGER manager( *python );

            // Inherit the full parent environment; see the note in the CREATE_ENV branch.
            wxExecuteEnv env;
            wxGetEnvMap( &env.env );

#ifdef _WIN32
            wxString systemRoot;
            wxGetEnv( wxS( "SYSTEMROOT" ), &systemRoot );
            env.env[wxS( "SYSTEMROOT" )] = systemRoot;

            // The venv\Scripts\python.exe launcher spawned by pip may resolve the VC
            // runtime DLLs through PATH; make sure the interpreter's directory is there.
            prependInterpreterDirToPath( env,
                                         Pgm().GetCommonSettings()->m_Api.python_interpreter );
            copyVcRuntimeToVenv( *python, Pgm().GetCommonSettings()->m_Api.python_interpreter );

            // If we are using the KiCad-shipped Python interpreter we have to do hacks
            env.env.erase( "PYTHONHOME" );
            env.env.erase( "PYTHONPATH" );
#endif

            if( pythonHome )
                env.env[wxS( "VIRTUAL_ENV" )] = *pythonHome;

            std::vector<wxString> args = {
                    "-m",
                    "pip",
                    "install",
                    "--no-input",
                    "--isolated",
                    "--only-binary",
                    ":all:",
                    "--require-virtualenv",
                    "--exists-action",
                    "i",
                    "-r",
                    reqs.GetFullPath()
                };

            long pid = manager.Execute( args,
                [this, job]( int aRetVal, const wxString& aOutput, const wxString& aError )
                {
                    m_runningPids.erase( job.identifier );

                    if( m_cancelledPlugins.contains( job.identifier ) )
                    {
                        recreateCancelledPlugin( job.identifier );
                        return;
                    }

                    if( !aError.IsEmpty() )
                        wxLogTrace( traceApi, wxString::Format( "Manager: pip stderr: %s", aError ) );

                    if( aRetVal != 0 )
                    {
                        wxString error = aError;
                        error.Trim().Trim( false );

                        if( error.IsEmpty() )
                            error = wxString::Format( _( "error code %d" ), aRetVal );

                        reportPluginLoadMessage( m_reloadReporter.get(), pluginName( job.identifier ),
                                                 _( "Could not create plugin environment" ), error );
                    }

                    if( aRetVal == 0 )
                    {
                        wxLogTrace( traceApi, wxString::Format( "Manager: marking %s as ready",
                                                                job.identifier ) );
                        m_readyPlugins.insert( job.identifier );

                        wxCommandEvent* availabilityEvt =
                                new wxCommandEvent( EDA_EVT_PLUGIN_AVAILABILITY_CHANGED, wxID_ANY );
                        wxTheApp->QueueEvent( availabilityEvt );
                    }

                    m_busyPlugins.erase( job.identifier );

                    wxCommandEvent* evt = new wxCommandEvent( EDA_EVT_PLUGIN_MANAGER_JOB_FINISHED,
                                                              wxID_ANY );

                    QueueEvent( evt );
                }, &env );

            m_runningPids[job.identifier] = pid;
        }

        wxCommandEvent* evt = new wxCommandEvent( EDA_EVT_PLUGIN_MANAGER_JOB_FINISHED, wxID_ANY );
        QueueEvent( evt );
    }

    m_jobs.pop_front();

    if( !Busy() )
        m_reloadReporter.reset();

    wxLogTrace( traceApi, wxString::Format( "Manager: finished job; %zu left in queue",
                                            m_jobs.size() ) );
}


bool API_PLUGIN_MANAGER::Busy() const
{
    return !m_jobs.empty() || !m_busyPlugins.empty();
}
