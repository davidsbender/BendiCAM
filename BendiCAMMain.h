/***************************************************************
 * Name:      BendiCAMMain.h
 * Purpose:   Defines Application Frame
 * Author:    David Bender (d_bender@bluewin.ch)
 * Created:   2026-02-12
 * Copyright: David Bender ()
 * License:
 **************************************************************/

#ifndef BENDICAMMAIN_H
#define BENDICAMMAIN_H

//(*Headers(BendiCAMFrame)
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/checklst.h>
#include <wx/frame.h>
#include <wx/menu.h>
#include <wx/panel.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
#include <wx/treectrl.h>
//*)

class PartTreeItemData : public wxTreeItemData {
public:
    int partIndex = -1;
    int shapeIndex = -1;
    PartTreeItemData(int pIdx, int sIdx = -1) : partIndex(pIdx), shapeIndex(sIdx) {}
};

#include <wx/filedlg.h>
#include <wx/dcclient.h>
#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include <wx/filename.h>
#include <wx/splitter.h>
#include <wx/sizer.h>
#include <fstream>
#include <memory>
#include <hidsdi.h>
#include <hidpi.h>
#include "DxfParser.h"
#include "JoystickCalibration.h"
#include "SteeringWheelCtrl.h"
#include "PositionBarCtrl.h"

struct CamShape {
    int shapeId = 1;
    double depth = 5.0;
    std::set<size_t> entityIndices;
};

struct CamPart {
    int id = 1;
    std::string name;
    std::vector<CamShape> shapes;
};

class BendiCAMFrame: public wxFrame
{
    public:

        BendiCAMFrame(wxWindow* parent,wxWindowID id = -1);
        virtual ~BendiCAMFrame();
        WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam);

    private:

        //(*Handlers(BendiCAMFrame)
        void OnAbout(wxCommandEvent& event);
        void OnOpen(wxCommandEvent& event);
        void OnOpenProject(wxCommandEvent& event);
        void OnSaveProject(wxCommandEvent& event);
        void OnTimerTrigger(wxTimerEvent& event);
        void OnQuit(wxCloseEvent& event);
        void OnClose(wxCloseEvent& event);
        void OnCheckListBoxLayersToggled(wxCommandEvent& event);
        void OnButtonCreatePartClick(wxCommandEvent& event);
        void OnButtonAddToPartClick(wxCommandEvent& event);
        void OnTreeCtrlPartsSelChanged(wxTreeEvent& event);
        void OnTextCtrlDepthChange(wxCommandEvent& event);
        void OnCheckBoxClosePolylinesClick(wxCommandEvent& event);
        //*)

        //(*Identifiers(BendiCAMFrame)
        static const wxWindowID ID_PANEL1;
        static const wxWindowID ID_PANEL2;
        static const wxWindowID ID_STATICBOX3;
        static const wxWindowID ID_CHECKLISTBOX1;
        static const wxWindowID ID_STATICBOX4;
        static const wxWindowID ID_TREECTRL1;
        static const wxWindowID ID_STATICTEXT1;
        static const wxWindowID ID_TEXTCTRL2;
        static const wxWindowID ID_BUTTON1;
        static const wxWindowID ID_BUTTON2;
        static const wxWindowID ID_CHECKBOX1;
        static const wxWindowID ID_STATICTEXT2;
        static const wxWindowID ID_TEXTCTRL3;
        static const wxWindowID ID_STATICBOX2;
        static const wxWindowID ID_TEXTCTRL1;
        static const wxWindowID idMenuOpen;
        static const wxWindowID idMenuOpenProject;
        static const wxWindowID idMenuSaveProject;
        static const wxWindowID ID_MENUITEM1;
        static const wxWindowID idMenuAbout;
        static const wxWindowID ID_TIMER;
        //*)

        //(*Declarations(BendiCAMFrame)
        wxButton* ButtonAddToPart;
        wxButton* ButtonCreatePart;
        wxCheckBox* CheckBoxClosePolylines;
        wxCheckListBox* CheckListBoxLayers;
        wxPanel* Panel3D;
        wxPanel* PanelDrawing;
        wxStaticBox* StaticBoxLayers;
        wxStaticBox* StaticBoxParts;
        wxStaticBox* StaticBoxStatus;
        wxStaticText* StaticTextConnTol;
        wxStaticText* StaticTextDepth;
        wxTextCtrl* TextCtrlConnTolerance;
        wxTextCtrl* TextCtrlDepth;
        wxTextCtrl* TextCtrlStatus;
        wxTimer Timer;
        wxTreeCtrl* TreeCtrlParts;
        //*)

        // Custom Splitter & Panel Members (outside wxSmith block so wxSmith never deletes them)
        wxSplitterWindow* SplitterMain = nullptr;
        wxSplitterWindow* SplitterTop = nullptr;
        wxSplitterWindow* SplitterDraw3D = nullptr;
        wxPanel* PanelTop = nullptr;
        wxPanel* PanelBottom = nullptr;
        wxPanel* PanelDrawAnd3D = nullptr;
        wxPanel* PanelRight = nullptr;

        void OnPanelPaint(wxPaintEvent& event);
        void OnPanelEraseBg(wxEraseEvent& event);
        void OnPanelMouseWheel(wxMouseEvent& event);
        void OnPanelMiddleDown(wxMouseEvent& event);
        void OnPanelMiddleUp(wxMouseEvent& event);
        void OnPanelMouseMove(wxMouseEvent& event);
        void OnPanelLeftDown(wxMouseEvent& event);

        void OnPanel3DPaint(wxPaintEvent& event);
        void OnPanel3DEraseBg(wxEraseEvent& event);
        void OnPanel3DLeftDown(wxMouseEvent& event);
        void OnPanel3DLeftUp(wxMouseEvent& event);
        void OnPanel3DMiddleDown(wxMouseEvent& event);
        void OnPanel3DMiddleUp(wxMouseEvent& event);
        void OnPanel3DMouseMove(wxMouseEvent& event);
        void OnPanel3DMouseWheel(wxMouseEvent& event);
        void OnSize(wxSizeEvent& event);
        void OnRightPanelSize(wxSizeEvent& event);
        void OnBottomPanelSize(wxSizeEvent& event);

        void UpdatePartButtonStates();
        void RebuildPartsTree();
        void getMinMaxDepth(double& minDepth, double& maxDepth) const;
        wxColour getDepthColor(double depth, double minDepth, double maxDepth) const;
        std::vector<std::vector<DxfPoint>> getShapeClosedPaths(const CamShape& shape) const;

        std::string m_currentDxfPath;
        double m_zoom = 1.0;
        double m_panX = 0.0;
        double m_panY = 0.0;
        bool m_isDragging = false;
        wxPoint m_lastMousePos;
        std::set<size_t> m_selectedIndices;

        // 3D Camera State
        double m_rotX3D = 30.0;
        double m_rotY3D = -45.0;
        double m_zoom3D = 1.0;
        double m_panX3D = 0.0;
        double m_panY3D = 0.0;
        bool m_isDragging3DRot = false;
        bool m_isDragging3DPan = false;
        wxPoint m_lastMousePos3D;

        std::vector<CamPart> m_parts;
        int m_selectedPartIndex = -1;
        int m_selectedShapeInPartIndex = -1;
        int m_nextPartId = 1;

        DxfModel m_dxfModel;
        PHIDP_PREPARSED_DATA preparsedData = nullptr;

        DECLARE_EVENT_TABLE()
};

#endif // BENDICAMMAIN_H
