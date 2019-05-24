/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2013 CERN
 * Copyright (C) 2017-2019 KiCad Developers, see AUTHORS.txt for contributors.
 * @author Jean-Pierre Charras, jp.charras at wanadoo.fr
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

#include <fctsys.h>
#include <kiface_i.h>
#include <base_units.h>
#include <msgpanel.h>
#include <bitmaps.h>
#include <eda_dockart.h>
#include <pl_editor_frame.h>
#include <pl_editor_id.h>
#include <pl_draw_panel_gal.cpp>
#include <hotkeys.h>
#include <pl_editor_screen.h>
#include <ws_draw_item.h>
#include <worksheet_dataitem.h>
#include <properties_frame.h>
#include <view/view.h>
#include <wildcards_and_files_ext.h>
#include <confirm.h>
#include <tool/selection.h>
#include <tool/tool_dispatcher.h>
#include <tool/tool_manager.h>
#include <tool/common_tools.h>
#include <tool/zoom_tool.h>
#include <tools/pl_actions.h>
#include <tools/pl_selection_tool.h>
#include <tools/pl_drawing_tools.h>
#include <tools/pl_edit_tool.h>
#include <tools/pl_point_editor.h>
#include <tools/pl_picker_tool.h>


PL_EDITOR_FRAME::PL_EDITOR_FRAME( KIWAY* aKiway, wxWindow* aParent ) :
        EDA_DRAW_FRAME( aKiway, aParent, FRAME_PL_EDITOR, wxT( "PlEditorFrame" ),
                        wxDefaultPosition, wxDefaultSize,
                        KICAD_DEFAULT_DRAWFRAME_STYLE, PL_EDITOR_FRAME_NAME )
{
    m_UserUnits = MILLIMETRES;
    m_zoomLevelCoeff = 290.0;   // Adjusted to roughly displays zoom level = 1
                                // when the screen shows a 1:1 image
                                // obviously depends on the monitor,
                                // but this is an acceptable value

    m_showAxis = false;                 // true to show X and Y axis on screen
    m_showGridAxis = true;
    m_showBorderAndTitleBlock   = true; // true for reference drawings.
    m_hotkeysDescrList = PlEditorHotkeysDescr;
    m_originSelectChoice = 0;
    SetDrawBgColor( WHITE );            // default value, user option (WHITE/BLACK)
    WORKSHEET_DATAITEM::m_SpecialMode = true;
    SetShowPageLimits( true );
    m_AboutTitle = "PlEditor";

    m_propertiesFrameWidth = 200;

    // Give an icon
    wxIcon icon;
    icon.CopyFromBitmap( KiBitmap( icon_pagelayout_editor_xpm ) );
    SetIcon( icon );

    // Create GAL canvas
#ifdef __WXMAC__
    // Cairo renderer doesn't handle Retina displays
    m_canvasType = EDA_DRAW_PANEL_GAL::GAL_TYPE_OPENGL;
#else
    m_canvasType = EDA_DRAW_PANEL_GAL::GAL_TYPE_CAIRO;
#endif

    auto* drawPanel = new PL_DRAW_PANEL_GAL( this, -1, wxPoint( 0, 0 ), m_FrameSize,
                                             GetGalDisplayOptions(), m_canvasType );
    SetGalCanvas( drawPanel );

    LoadSettings( config() );
    SetSize( m_FramePos.x, m_FramePos.y, m_FrameSize.x, m_FrameSize.y );

    wxSize pageSizeIU = GetPageLayout().GetPageSettings().GetSizeIU();
    SetScreen( new PL_EDITOR_SCREEN( pageSizeIU ) );

    if( !GetScreen()->GridExists( m_LastGridSizeId + ID_POPUP_GRID_LEVEL_1000 ) )
        m_LastGridSizeId = ID_POPUP_GRID_LEVEL_1MM - ID_POPUP_GRID_LEVEL_1000;

    GetScreen()->SetGrid( m_LastGridSizeId + ID_POPUP_GRID_LEVEL_1000 );

    setupTools();
    ReCreateMenuBar();
    ReCreateHToolbar();
    ReCreateVToolbar();

    wxWindow* stsbar = GetStatusBar();
    int dims[] = {

        // balance of status bar on far left is set to a default or whatever is left over.
        -1,

        // When using GetTextSize() remember the width of '1' is not the same
        // as the width of '0' unless the font is fixed width, and it usually won't be.

        // zoom:
        GetTextSize( wxT( "Z 762000" ), stsbar ).x + 10,

        // cursor coords
        GetTextSize( wxT( "X 0234.567  Y 0234.567" ), stsbar ).x + 10,

        // delta distances
        GetTextSize( wxT( "dx 0234.567  dx 0234.567" ), stsbar ).x + 10,

        // Coord origin (use the bigger message)
        GetTextSize( _( "coord origin: Right Bottom page corner" ), stsbar ).x + 10,

        // units display, Inches is bigger than mm
        GetTextSize( _( "Inches" ), stsbar ).x + 10
    };

    SetStatusWidths( arrayDim( dims ), dims );

    m_auimgr.SetManagedWindow( this );
    m_auimgr.SetArtProvider( new EDA_DOCKART( this ) );

    m_propertiesPagelayout = new PROPERTIES_FRAME( this );

    // Horizontal items; layers 4 - 6
    m_auimgr.AddPane( m_mainToolBar, EDA_PANE().HToolbar().Name( "MainToolbar" ).Top().Layer(6) );
    m_auimgr.AddPane( m_messagePanel, EDA_PANE().Messages().Name( "MsgPanel" ).Bottom().Layer(6) );

    // Vertical items; layers 1 - 3
    m_auimgr.AddPane( m_drawToolBar, EDA_PANE().VToolbar().Name( "ToolsToolbar" ).Right().Layer(1) );

    m_auimgr.AddPane( m_propertiesPagelayout, EDA_PANE().Palette().Name( "Props" ).Right().Layer(2)
                      .Caption( _( "Properties" ) ).MinSize( m_propertiesPagelayout->GetMinSize() )
                      .BestSize( m_propertiesFrameWidth, -1 ) );

    m_auimgr.AddPane( m_canvas, EDA_PANE().Canvas().Name( "DrawFrame" ).Center() );
    m_auimgr.AddPane( GetGalCanvas(), EDA_PANE().Canvas().Name( "DrawFrameGal" ).Center().Hide() );

    UseGalCanvas( true );

    m_auimgr.Update();

    // Initialize the current page layout
    WORKSHEET_LAYOUT& pglayout = WORKSHEET_LAYOUT::GetTheInstance();
#if 0       //start with empty layout
    pglayout.AllowVoidList( true );
    pglayout.ClearList();
#else       // start with the default Kicad layout
    pglayout.SetPageLayout();
#endif
    OnNewPageLayout();
}


void PL_EDITOR_FRAME::setupTools()
{
    // Create the manager and dispatcher & route draw panel events to the dispatcher
    m_toolManager = new TOOL_MANAGER;
    m_toolManager->SetEnvironment( nullptr, GetGalCanvas()->GetView(),
                                   GetGalCanvas()->GetViewControls(), this );
    m_actions = new PL_ACTIONS();
    m_toolDispatcher = new TOOL_DISPATCHER( m_toolManager, m_actions );

    GetGalCanvas()->SetEventDispatcher( m_toolDispatcher );

    // Register tools
    m_toolManager->RegisterTool( new COMMON_TOOLS );
    m_toolManager->RegisterTool( new ZOOM_TOOL );
    m_toolManager->RegisterTool( new PL_SELECTION_TOOL );
    m_toolManager->RegisterTool( new PL_DRAWING_TOOLS );
    m_toolManager->RegisterTool( new PL_EDIT_TOOL );
    m_toolManager->RegisterTool( new PL_POINT_EDITOR );
    m_toolManager->RegisterTool( new PL_PICKER_TOOL );
    m_toolManager->InitTools();

    // Run the selection tool, it is supposed to be always active
    m_toolManager->InvokeTool( "plEditor.InteractiveSelection" );
}


bool PL_EDITOR_FRAME::OpenProjectFiles( const std::vector<wxString>& aFileSet, int aCtl )
{
    wxString fn = aFileSet[0];

    if( !LoadPageLayoutDescrFile( fn ) )
    {
        wxMessageBox( wxString::Format( _( "Error when loading file \"%s\"" ), fn ) );
        return false;
    }
    else
    {
        OnNewPageLayout();
        return true;
    }
}


void PL_EDITOR_FRAME::OnCloseWindow( wxCloseEvent& Event )
{
    if( GetScreen()->IsModify() )
    {
        if( !HandleUnsavedChanges( this,
                                   _( "The current page layout has been modified. Save changes?" ),
                                   [&]()->bool { return saveCurrentPageLayout(); } ) )
        {
            Event.Veto();
            return;
        }
    }

    // do not show the window because we do not want any paint event
    Show( false );

    // was: Pgm().SaveCurrentSetupValues( m_configSettings );
    wxConfigSaveSetups( Kiface().KifaceSettings(), m_configSettings );

    // On Linux, m_propertiesPagelayout must be destroyed
    // before deleting the main frame to avoid a crash when closing
    m_propertiesPagelayout->Destroy();
    Destroy();
}


const BOX2I PL_EDITOR_FRAME::GetDocumentExtents() const
{
    BOX2I rv( VECTOR2I( 0, 0 ), GetPageLayout().GetPageSettings().GetSizeIU() );
    return rv;
}


double PL_EDITOR_FRAME::BestZoom()
{
    double  sizeX = (double) GetPageLayout().GetPageSettings().GetWidthIU();
    double  sizeY = (double) GetPageLayout().GetPageSettings().GetHeightIU();
    wxPoint centre( KiROUND( sizeX / 2 ), KiROUND( sizeY / 2 ) );

    // The sheet boundary already affords us some margin, so add only an
    // additional 5%.
    double margin_scale_factor = 1.05;

    return bestZoom( sizeX, sizeY, margin_scale_factor, centre );
}


static const wxChar propertiesFrameWidthKey[] = wxT( "PropertiesFrameWidth" );
static const wxChar cornerOriginChoiceKey[] = wxT( "CornerOriginChoice" );
static const wxChar blackBgColorKey[] = wxT( "BlackBgColor" );


void PL_EDITOR_FRAME::LoadSettings( wxConfigBase* aCfg )
{
    EDA_DRAW_FRAME::LoadSettings( aCfg );

    aCfg->Read( propertiesFrameWidthKey, &m_propertiesFrameWidth, 150);
    aCfg->Read( cornerOriginChoiceKey, &m_originSelectChoice );
    bool tmp;
    aCfg->Read( blackBgColorKey, &tmp, false );
    SetDrawBgColor( tmp ? BLACK : WHITE );
}


void PL_EDITOR_FRAME::SaveSettings( wxConfigBase* aCfg )
{
    EDA_DRAW_FRAME::SaveSettings( aCfg );

    m_propertiesFrameWidth = m_propertiesPagelayout->GetSize().x;

    aCfg->Write( propertiesFrameWidthKey, m_propertiesFrameWidth);
    aCfg->Write( cornerOriginChoiceKey, m_originSelectChoice );
    aCfg->Write( blackBgColorKey, GetDrawBgColor() == BLACK );

    // was: wxGetApp().SaveCurrentSetupValues( GetConfigurationSettings() );
    wxConfigSaveSetups( aCfg, GetConfigurationSettings() );
}


void PL_EDITOR_FRAME::UpdateTitleAndInfo()
{
    wxString title;
    wxString file = GetCurrFileName();

    title.Printf( _( "Page Layout Editor" ) + wxT( " \u2014 %s" ),
                  file.Length() ? file : _( "no file selected" ) );
    SetTitle( title );
}


const wxString& PL_EDITOR_FRAME::GetCurrFileName() const
{
    return BASE_SCREEN::m_PageLayoutDescrFileName;
}


void PL_EDITOR_FRAME::SetCurrFileName( const wxString& aName )
{
    BASE_SCREEN::m_PageLayoutDescrFileName = aName;
}


void PL_EDITOR_FRAME::SetPageSettings( const PAGE_INFO& aPageSettings )
{
    m_pageLayout.SetPageSettings( aPageSettings );

    if( GetScreen() )
        GetScreen()->InitDataPoints( aPageSettings.GetSizeIU() );
}


const PAGE_INFO& PL_EDITOR_FRAME::GetPageSettings() const
{
    return m_pageLayout.GetPageSettings();
}


const wxSize PL_EDITOR_FRAME::GetPageSizeIU() const
{
    // this function is only needed because EDA_DRAW_FRAME is not compiled
    // with either -DPCBNEW or -DEESCHEMA, so the virtual is used to route
    // into an application specific source file.
    return m_pageLayout.GetPageSettings().GetSizeIU();
}


const TITLE_BLOCK& PL_EDITOR_FRAME::GetTitleBlock() const
{
    return GetPageLayout().GetTitleBlock();
}


void PL_EDITOR_FRAME::SetTitleBlock( const TITLE_BLOCK& aTitleBlock )
{
    m_pageLayout.SetTitleBlock( aTitleBlock );
}


void PL_EDITOR_FRAME::UpdateStatusBar()
{
    PL_EDITOR_SCREEN* screen = (PL_EDITOR_SCREEN*) GetScreen();

    if( !screen )
        return;

    // Display Zoom level:
    EDA_DRAW_FRAME::UpdateStatusBar();

    // coordinate origin can be the paper Top Left corner,
    // or each of 4 page corners
    // We know the origin, and the orientation of axis
    wxPoint originCoord;
    int Xsign = 1;
    int Ysign = 1;

    WORKSHEET_DATAITEM dummy( WORKSHEET_DATAITEM::WS_SEGMENT );

    switch( m_originSelectChoice )
    {
    default:
    case 0: // Origin = paper Left Top corner
        break;

    case 1: // Origin = page Right Bottom corner
        Xsign = -1;
        Ysign = -1;
        dummy.SetStart( 0, 0, RB_CORNER );
        originCoord = dummy.GetStartPosUi();
        break;

    case 2: // Origin = page Left Bottom corner
        Ysign = -1;
        dummy.SetStart( 0, 0, LB_CORNER );
        originCoord = dummy.GetStartPosUi();
        break;

    case 3: // Origin = page Right Top corner
        Xsign = -1;
        dummy.SetStart( 0, 0, RT_CORNER );
        originCoord = dummy.GetStartPosUi();
        break;

    case 4: // Origin = page Left Top corner
        dummy.SetStart( 0, 0, LT_CORNER );
        originCoord = dummy.GetStartPosUi();
        break;
    }

    SetGridOrigin( originCoord );

    // Display absolute coordinates:
    wxPoint coord = GetCrossHairPosition() - originCoord;
    double dXpos = To_User_Unit( GetUserUnits(), coord.x*Xsign );
    double dYpos = To_User_Unit( GetUserUnits(), coord.y*Ysign );

    wxString pagesizeformatter = _( "Page size: width %.4g height %.4g" );
    wxString absformatter = wxT( "X %.4g  Y %.4g" );
    wxString locformatter = wxT( "dx %.4g  dy %.4g" );

    switch( GetUserUnits() )
    {
    case INCHES:         SetStatusText( _("inches"), 5 );   break;
    case MILLIMETRES:    SetStatusText( _("mm"), 5 );       break;
    case UNSCALED_UNITS: SetStatusText( wxEmptyString, 5 ); break;
    case DEGREES:        wxASSERT( false );                 break;
    }

    wxString line;

    // Display page size
    #define MILS_TO_MM (25.4/1000)
    DSIZE size = GetPageSettings().GetSizeMils();
    size = size * MILS_TO_MM;
    line.Printf( pagesizeformatter, size.x, size.y );
    SetStatusText( line, 0 );

    // Display abs coordinates
    line.Printf( absformatter, dXpos, dYpos );
    SetStatusText( line, 2 );

    // Display relative coordinates:
    int dx = GetCrossHairPosition().x - screen->m_O_Curseur.x;
    int dy = GetCrossHairPosition().y - screen->m_O_Curseur.y;
    dXpos = To_User_Unit( GetUserUnits(), dx * Xsign );
    dYpos = To_User_Unit( GetUserUnits(), dy * Ysign );
    line.Printf( locformatter, dXpos, dYpos );
    SetStatusText( line, 3 );

    // Display corner reference for coord origin
    line.Printf( _("coord origin: %s"),
                m_originSelectBox->GetString( m_originSelectChoice ). GetData() );
    SetStatusText( line, 4 );

    // Display units
}


void PL_EDITOR_FRAME::PrintPage( wxDC* aDC, LSET , bool , void *  )
{
    GetScreen()-> m_ScreenNumber = GetPageNumberOption() ? 1 : 2;
    DrawWorkSheet( aDC, GetScreen(), 0, IU_PER_MILS, wxEmptyString );
}


void PL_EDITOR_FRAME::RedrawActiveWindow( wxDC* aDC, bool aEraseBg )
{
    // JEY TODO: probably obsolete unless called for print....
    GetScreen()-> m_ScreenNumber = GetPageNumberOption() ? 1 : 2;

    if( aEraseBg )
        m_canvas->EraseScreen( aDC );

    m_canvas->DrawBackGround( aDC );

    WS_DRAW_ITEM_LIST& drawList = GetPageLayout().GetDrawItems();

    drawList.SetDefaultPenSize( 0 );
    drawList.SetMilsToIUfactor( IU_PER_MILS );
    drawList.SetSheetNumber( GetScreen()->m_ScreenNumber );
    drawList.SetSheetCount( GetScreen()->m_NumberOfScreens );
    drawList.SetFileName( GetCurrFileName() );
    drawList.SetSheetName( GetScreenDesc() );
    drawList.SetSheetLayer( wxEmptyString );

    // Draw item list
    drawList.Draw( m_canvas->GetClipBox(), aDC, DARKMAGENTA );

#ifdef USE_WX_OVERLAY
    if( IsShown() )
    {
        m_overlay.Reset();
        wxDCOverlay overlaydc( m_overlay, (wxWindowDC*)aDC );
        overlaydc.Clear();
    }
#endif

    if( m_canvas->IsMouseCaptured() )
        m_canvas->CallMouseCapture( aDC, wxDefaultPosition, false );

    m_canvas->DrawCrossHair( aDC );

    // Display the filename
    UpdateTitleAndInfo();
}


void PL_EDITOR_FRAME::HardRedraw()
{
    PL_DRAW_PANEL_GAL*  drawPanel = static_cast<PL_DRAW_PANEL_GAL*>( GetGalCanvas() );

    drawPanel->DisplayWorksheet();

    PL_SELECTION_TOOL*  selTool = m_toolManager->GetTool<PL_SELECTION_TOOL>();
    SELECTION&          selection = selTool->GetSelection();
    WORKSHEET_DATAITEM* item = nullptr;

    if( selection.GetSize() == 1 )
        item = static_cast<WS_DRAW_ITEM_BASE*>( selection.Front() )->GetPeer();

    m_propertiesPagelayout->CopyPrmsFromItemToPanel( item );
    m_propertiesPagelayout->CopyPrmsFromGeneralToPanel();
    m_canvas->Refresh();
}


WORKSHEET_DATAITEM* PL_EDITOR_FRAME::AddPageLayoutItem( int aType )
{
    WORKSHEET_DATAITEM * item = NULL;

    switch( aType )
    {
    case WORKSHEET_DATAITEM::WS_TEXT:
        item = new WORKSHEET_DATAITEM_TEXT( wxT( "Text") );
        break;

    case WORKSHEET_DATAITEM::WS_SEGMENT:
        item = new WORKSHEET_DATAITEM( WORKSHEET_DATAITEM::WS_SEGMENT );
        break;

    case WORKSHEET_DATAITEM::WS_RECT:
        item = new WORKSHEET_DATAITEM( WORKSHEET_DATAITEM::WS_RECT );
        break;

    case WORKSHEET_DATAITEM::WS_POLYPOLYGON:
        item = new WORKSHEET_DATAITEM_POLYPOLYGON();
        break;

    case WORKSHEET_DATAITEM::WS_BITMAP:
    {
        wxFileDialog fileDlg( this, _( "Choose Image" ), wxEmptyString, wxEmptyString,
                              _( "Image Files " ) + wxImage::GetImageExtWildcard(), wxFD_OPEN );

        if( fileDlg.ShowModal() != wxID_OK )
            return NULL;

        wxString fullFilename = fileDlg.GetPath();

        if( !wxFileExists( fullFilename ) )
        {
            wxMessageBox( _( "Couldn't load image from \"%s\"" ), fullFilename );
            break;
        }

        BITMAP_BASE* image = new BITMAP_BASE();

        if( !image->ReadImageFile( fullFilename ) )
        {
            wxMessageBox( _( "Couldn't load image from \"%s\"" ), fullFilename );
            delete image;
            break;
        }

        item = new WORKSHEET_DATAITEM_BITMAP( image );
    }
    break;
    }

    if( item == NULL )
        return NULL;

    WORKSHEET_LAYOUT::GetTheInstance().Append( item );
    item->SyncDrawItems( nullptr, GetGalCanvas()->GetView() );

    return item;
}


void PL_EDITOR_FRAME::OnNewPageLayout()
{
    GetScreen()->ClearUndoRedoList();
    GetScreen()->ClrModify();

    static_cast<PL_DRAW_PANEL_GAL*>( GetGalCanvas() )->DisplayWorksheet();

    m_propertiesPagelayout->CopyPrmsFromGeneralToPanel();
    m_toolManager->RunAction( ACTIONS::zoomFitScreen, true );
    m_canvas->Refresh();
}


const wxString PL_EDITOR_FRAME::GetZoomLevelIndicator() const
{
    return EDA_DRAW_FRAME::GetZoomLevelIndicator();
}
