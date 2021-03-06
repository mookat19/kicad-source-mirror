/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2011 Jean-Pierre Charras, jean-pierre.charras@ujf-grenoble.fr
 * Copyright (C) 2009 Dick Hollenbeck, dick@softplc.com
 * Copyright (C) 2004-2020 KiCad Developers, see AUTHORS.txt for contributors.
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


#ifndef _DIALOG_DRC_H_
#define _DIALOG_DRC_H_

#include <wx/htmllbox.h>
#include <fctsys.h>
#include <pcbnew.h>
#include <drc/drc.h>
#include <rc_item.h>
#include <class_marker_pcb.h>
#include <class_board.h>
#include <dialog_drc_base.h>
#include <widgets/unit_binder.h>


class BOARD_DESIGN_SETTINGS;


#define DIALOG_DRC_WINDOW_NAME "DialogDrcWindowName"

class
DIALOG_DRC: public DIALOG_DRC_BASE
{
public:
    /// Constructors
    DIALOG_DRC( DRC* aTester, PCB_EDIT_FRAME* aEditorFrame, wxWindow* aParent );
    ~DIALOG_DRC();

    void SetMarkersProvider( RC_ITEMS_PROVIDER* aProvider );
    void SetUnconnectedProvider( RC_ITEMS_PROVIDER* aProvider );
    void SetFootprintsProvider( RC_ITEMS_PROVIDER* aProvider );

private:
    /**
     * Function writeReport
     * outputs the MARKER items and unconnecte DRC_ITEMs with commentary to an
     * open text file.
     * @param aFullFileName The text filename to write the report to.
     * @return true if OK, false on error
     */
    bool writeReport( const wxString& aFullFileName );

    void initValues();
    void displayDRCValues();
    void setDRCParameters();
    void syncCheckboxes();
    void updateDisplayedCounts();

    void OnDRCItemSelected( wxDataViewEvent& aEvent ) override;
    void OnDRCItemDClick( wxDataViewEvent& aEvent ) override;
    void OnDRCItemRClick( wxDataViewEvent& aEvent ) override;

    void OnSeverity( wxCommandEvent& aEvent ) override;
  	void OnSaveReport( wxCommandEvent& aEvent ) override;

    void OnDeleteOneClick( wxCommandEvent& aEvent ) override;
    void OnDeleteAllClick( wxCommandEvent& aEvent ) override;
    void OnRunDRCClick( wxCommandEvent& aEvent ) override;
    void OnCancelClick( wxCommandEvent& aEvent ) override;

    /// handler for activate event, updating data which can be modified outside the dialog
    /// (DRC parameters)
    void OnActivateDlg( wxActivateEvent& aEvent ) override;

    void OnChangingNotebookPage( wxNotebookEvent& aEvent ) override;

    void deleteAllMarkers();
    void refreshBoardEditor();

    BOARD_DESIGN_SETTINGS& bds() { return m_currentBoard->GetDesignSettings(); }

    BOARD*             m_currentBoard;     // the board currently on test
    DRC*               m_tester;
    PCB_EDIT_FRAME*    m_brdEditor;

    wxString           m_markersTitleTemplate;
    wxString           m_unconnectedTitleTemplate;
    wxString           m_footprintsTitleTemplate;

    UNIT_BINDER        m_trackMinWidth;
    UNIT_BINDER        m_viaMinSize;
    UNIT_BINDER        m_uviaMinSize;

    RC_ITEMS_PROVIDER* m_markersProvider;
    RC_TREE_MODEL*     m_markerTreeModel;

    RC_ITEMS_PROVIDER* m_unconnectedItemsProvider;
    RC_TREE_MODEL*     m_unconnectedTreeModel;

    RC_ITEMS_PROVIDER* m_footprintWarningsProvider;
    RC_TREE_MODEL*     m_footprintWarningsTreeModel;

    int                m_severities;
};

#endif  // _DIALOG_DRC_H_

