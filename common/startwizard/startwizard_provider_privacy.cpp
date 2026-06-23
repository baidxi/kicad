/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2023 Mark Roszko <mark.roszko@gmail.com>
 * Copyright (C) 1992-2023 KiCad Developers, see AUTHORS.txt for contributors.
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

#include <app_monitor.h>
#include <pgm_base.h>
#include <settings/common_settings.h>
#include <settings/kicad_settings.h>

#include <startwizard/startwizard_provider_privacy.h>
#include <dialogs/panel_startwizard_privacy_base.h>
#include <settings/settings_manager.h>


class PANEL_STARTWIZARD_PRIVACY : public PANEL_STARTWIZARD_PRIVACY_BASE
{
public:
    PANEL_STARTWIZARD_PRIVACY( std::shared_ptr<STARTWIZARD_PROVIDER_PRIVACY_MODEL> aModel,
                               wxWindow* aParent ) :
            PANEL_STARTWIZARD_PRIVACY_BASE( aParent ),
            m_model( aModel )
    {
#ifndef KICAD_UPDATE_CHECK
        m_cbAutoUpdateKiCad->Hide();
#endif

#ifndef KICAD_PCM_UPDATE_CHECK
        m_cbAutoUpdatePCM->Hide();
#endif

#if !defined( KICAD_UPDATE_CHECK ) && !defined( KICAD_PCM_UPDATE_CHECK )
        if( wxSizer* updatesSizer = m_stIntro->GetContainingSizer() )
        {
            static_cast<wxStaticBoxSizer*>( updatesSizer )->GetStaticBox()->Hide();

            if( wxSizer* panelSizer = GetSizer() )
            {
                panelSizer->Hide( updatesSizer, true );
                panelSizer->Layout();
            }
        }
#endif

#ifndef KICAD_USE_SENTRY
        m_sizerDataCollection->Show( false );
#endif
    };

    bool TransferDataFromWindow() override
    {
#ifdef KICAD_UPDATE_CHECK
        m_model->m_autoUpdateKiCad = m_cbAutoUpdateKiCad->GetValue();
#endif

#ifdef KICAD_PCM_UPDATE_CHECK
        m_model->m_autoUpdatePCM   = m_cbAutoUpdatePCM->GetValue();
#endif

#ifdef KICAD_USE_SENTRY
        m_model->m_enableSentry    = m_cbDataCollection->GetValue();
#else
        m_model->m_enableSentry    = false;
#endif

        return true;
    }

    bool TransferDataToWindow() override
    {
#ifdef KICAD_UPDATE_CHECK
        m_cbAutoUpdateKiCad->SetValue( m_model->m_autoUpdateKiCad );
#endif

#ifdef KICAD_PCM_UPDATE_CHECK
        m_cbAutoUpdatePCM->SetValue( m_model->m_autoUpdatePCM );
#endif

#ifdef KICAD_USE_SENTRY
        m_cbDataCollection->SetValue( m_model->m_enableSentry );
#endif

        return true;
    }

private:
    std::shared_ptr<STARTWIZARD_PROVIDER_PRIVACY_MODEL> m_model;
};


STARTWIZARD_PROVIDER_PRIVACY::STARTWIZARD_PROVIDER_PRIVACY() :
        STARTWIZARD_PROVIDER( _( "Updates && Privacy" ) )
{
}


bool STARTWIZARD_PROVIDER_PRIVACY::NeedsUserInput() const
{
#if defined( KICAD_UPDATE_CHECK ) || defined( KICAD_PCM_UPDATE_CHECK ) || defined( KICAD_USE_SENTRY )
    COMMON_SETTINGS* commonSettings = Pgm().GetCommonSettings();

    bool needsInput = false;

#if defined( KICAD_UPDATE_CHECK ) || defined( KICAD_PCM_UPDATE_CHECK )
    needsInput |= !commonSettings->m_DoNotShowAgain.update_check_prompt;
#endif

#ifdef KICAD_USE_SENTRY
    needsInput |= !commonSettings->m_DoNotShowAgain.data_collection_prompt;
#endif

    return needsInput;
#else
    return false;
#endif
}


wxPanel* STARTWIZARD_PROVIDER_PRIVACY::GetWizardPanel( wxWindow* aParent, STARTWIZARD* aWizard )
{
    m_model = std::make_shared<STARTWIZARD_PROVIDER_PRIVACY_MODEL>();
    m_model->m_autoUpdateKiCad = KICAD_SETTINGS::DEFAULT_KICAD_UPDATE_CHECK;
    m_model->m_autoUpdatePCM = KICAD_SETTINGS::DEFAULT_PCM_UPDATE_CHECK;
    return new PANEL_STARTWIZARD_PRIVACY( m_model, aParent );
}


void STARTWIZARD_PROVIDER_PRIVACY::Finish()
{
    [[maybe_unused]] COMMON_SETTINGS* commonSettings = Pgm().GetCommonSettings();

#if defined( KICAD_UPDATE_CHECK ) || defined( KICAD_PCM_UPDATE_CHECK )
    KICAD_SETTINGS* settings = Pgm().GetSettingsManager().GetAppSettings<KICAD_SETTINGS>( "kicad" );
#endif

#ifdef KICAD_UPDATE_CHECK
    settings->m_KiCadUpdateCheck = m_model->m_autoUpdateKiCad;
#endif

#ifdef KICAD_PCM_UPDATE_CHECK
    settings->m_PcmUpdateCheck = m_model->m_autoUpdatePCM;
#endif

#if defined( KICAD_UPDATE_CHECK ) || defined( KICAD_PCM_UPDATE_CHECK )
    commonSettings->m_DoNotShowAgain.update_check_prompt = true;
#endif

#ifdef KICAD_USE_SENTRY
    APP_MONITOR::SENTRY::Instance()->SetSentryOptIn( m_model->m_enableSentry );
    commonSettings->m_DoNotShowAgain.data_collection_prompt = true;
#endif

    Pgm().GetSettingsManager().Save();
}


void STARTWIZARD_PROVIDER_PRIVACY::ApplyDefaults()
{
    [[maybe_unused]] COMMON_SETTINGS* commonSettings = Pgm().GetCommonSettings();

#if defined( KICAD_UPDATE_CHECK ) || defined( KICAD_PCM_UPDATE_CHECK )
    KICAD_SETTINGS* settings = Pgm().GetSettingsManager().GetAppSettings<KICAD_SETTINGS>( "kicad" );
#endif

#ifdef KICAD_UPDATE_CHECK
    settings->m_KiCadUpdateCheck = KICAD_SETTINGS::DEFAULT_KICAD_UPDATE_CHECK;
#endif

#ifdef KICAD_PCM_UPDATE_CHECK
    settings->m_PcmUpdateCheck = KICAD_SETTINGS::DEFAULT_PCM_UPDATE_CHECK;
#endif

#ifdef KICAD_USE_SENTRY
    APP_MONITOR::SENTRY::Instance()->SetSentryOptIn( false );
#endif

#if defined( KICAD_UPDATE_CHECK ) || defined( KICAD_PCM_UPDATE_CHECK )
    commonSettings->m_DoNotShowAgain.update_check_prompt = true;
#endif

#ifdef KICAD_USE_SENTRY
    commonSettings->m_DoNotShowAgain.data_collection_prompt = true;
#endif

    Pgm().SaveCommonSettings();
}
