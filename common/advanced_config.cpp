/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2018 KiCad Developers, see AUTHORS.txt for contributors.
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

#include <advanced_config.h>

#include <common.h>
#include <config_params.h>

#include <wx/config.h>
#include <wx/filename.h>
#include <wx/log.h>

/*
 * Flag to enable advanced config debugging
 *
 * Use "KICAD_ADVANCED_CONFIG" to enable.
 *
 * @ingroup trace_env_vars
 */
static const wxChar AdvancedConfigMask[] = wxT( "KICAD_ADVANCED_CONFIG" );

/**
 * List of known keys for advanced configuration options.
 *
 * Set these options in the file `kicad_advanced` in the
 * KiCad config directory.
 */
namespace AC_KEYS
{
} // namespace KEYS


/*
 * Get a simple string for common parameters.
 *
 * This isn't exhaustive, but it covers most common types that might be
 * used in the advance config
 */
wxString dumpParamCfg( const PARAM_CFG_BASE& aParam )
{
    wxString s = aParam.m_Ident + ": ";

    /*
     * This implementation is rather simplistic, but it is
     * effective enough for simple uses. A better implementation would be
     * some kind of visitor, but that's somewhat more work.
     */
    switch( aParam.m_Type )
    {
    case paramcfg_id::PARAM_INT:
    case paramcfg_id::PARAM_INT_WITH_SCALE:
        s << *static_cast<const PARAM_CFG_INT&>( aParam ).m_Pt_param;
        break;
    case paramcfg_id::PARAM_DOUBLE:
        s << *static_cast<const PARAM_CFG_DOUBLE&>( aParam ).m_Pt_param;
        break;
    case paramcfg_id::PARAM_WXSTRING:
        s << *static_cast<const PARAM_CFG_WXSTRING&>( aParam ).m_Pt_param;
        break;
    case paramcfg_id::PARAM_FILENAME:
        s << *static_cast<const PARAM_CFG_FILENAME&>( aParam ).m_Pt_param;
        break;
    case paramcfg_id::PARAM_BOOL:
        s << ( *static_cast<const PARAM_CFG_BOOL&>( aParam ).m_Pt_param ? "true" : "false" );
        break;
    default: s << "Unsupported PARAM_CFG variant: " << aParam.m_Type;
    }

    return s;
}


/**
 * Dump the configs in the given array to trace.
 */
static void dumpCfg( const PARAM_CFG_ARRAY& aArray )
{
    // only dump if we need to
    if( !wxLog::IsAllowedTraceMask( AdvancedConfigMask ) )
        return;

    for( const auto& param : aArray )
    {
        wxLogTrace( AdvancedConfigMask, dumpParamCfg( param ) );
    }
}


/**
 * Get the filename for the advanced config file
 *
 * The user must check the file exists if they care.
 */
static wxFileName getAdvancedCfgFilename()
{
    const static wxString cfg_filename{ "kicad_advanced" };
    return wxFileName( GetKicadConfigPath(), cfg_filename );
}


ADVANCED_CFG::ADVANCED_CFG()
{
    wxLogTrace( AdvancedConfigMask, "Init advanced config" );
    loadFromConfigFile();
}


const ADVANCED_CFG& ADVANCED_CFG::GetCfg()
{
    static ADVANCED_CFG instance;
    return instance;
}


void ADVANCED_CFG::loadFromConfigFile()
{
    const auto k_advanced = getAdvancedCfgFilename();

    if( !k_advanced.FileExists() )
    {
        wxLogTrace( AdvancedConfigMask, "File does not exist %s", k_advanced.GetFullPath() );
        return;
    }

    wxLogTrace( AdvancedConfigMask, "Loading advanced config from: %s", k_advanced.GetFullPath() );

    wxFileConfig file_cfg( "", "", k_advanced.GetFullPath() );
    loadSettings( file_cfg );
}


void ADVANCED_CFG::loadSettings( wxConfigBase& aCfg )
{
    PARAM_CFG_ARRAY configParams;

    // Add configs here

    wxConfigLoadSetups( &aCfg, configParams );

    dumpCfg( configParams );
}