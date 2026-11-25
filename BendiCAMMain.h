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

class MachiningTreeItemData : public wxTreeItemData {
public:
    int partIndex = -1;
    int levelIndex = -1;
    double depth = 0.0;
    std::vector<int> shapeIndices;
    MachiningTreeItemData(int pIdx, int lvlIdx = -1, double d = 0.0, const std::vector<int>& sIndices = {})
        : partIndex(pIdx), levelIndex(lvlIdx), depth(d), shapeIndices(sIndices) {}
};

#include <wx/filedlg.h>
#include <wx/dcclient.h>
#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include <wx/filename.h>
#include <wx/splitter.h>
#include <wx/sizer.h>
#include <wx/notebook.h>
#include <fstream>
#include <memory>
#include <hidsdi.h>
#include <hidpi.h>
#include "DxfParser.h"
#include "AnalyticalToolpath.h"
#include "MachiningPaths.h"
#include "JoystickCalibration.h"
#include "SteeringWheelCtrl.h"
#include "PositionBarCtrl.h"

class BendiCAMFrame: public wxFrame
{
    public:

        BendiCAMFrame(wxWindow* parent,wxWindowID id = -1);
        virtual ~BendiCAMFrame();
        WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam);

        bool OpenDxf(const wxString& filename);
        bool OpenProject(const wxString& projectPath);
        void AutoCreatePartsFromDxf();
        bool ExportGCodeToFile(const wxString& filePath);

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
        void OnExportGCode(wxCommandEvent& event);
        void OnToolOrMaterialSettingChange(wxCommandEvent& event);
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
        static const wxWindowID ID_STATICBOX5;
        static const wxWindowID ID_STATICTEXT3;
        static const wxWindowID ID_TEXTCTRL4;
        static const wxWindowID ID_STATICTEXT4;
        static const wxWindowID ID_TEXTCTRL5;
        static const wxWindowID ID_STATICTEXT5;
        static const wxWindowID ID_TEXTCTRL6;
        static const wxWindowID ID_STATICTEXT6;
        static const wxWindowID ID_TEXTCTRL7;
        static const wxWindowID ID_STATICTEXT7;
        static const wxWindowID ID_TEXTCTRL8;
        static const wxWindowID ID_STATICTEXT8;
        static const wxWindowID ID_TEXTCTRL9;
        static const wxWindowID ID_STATICTEXT9;
        static const wxWindowID ID_TEXTCTRL10;
        static const wxWindowID ID_STATICTEXT12;
        static const wxWindowID ID_TEXTCTRL13;
        static const wxWindowID ID_STATICBOX6;
        static const wxWindowID ID_STATICTEXT10;
        static const wxWindowID ID_TEXTCTRL11;
        static const wxWindowID ID_STATICTEXT11;
        static const wxWindowID ID_TEXTCTRL12;
        static const wxWindowID ID_BUTTON3;
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
        wxButton* ButtonExportGCode;
        wxCheckBox* CheckBoxClosePolylines;
        wxCheckListBox* CheckListBoxLayers;
        wxPanel* Panel3D;
        wxPanel* PanelDrawing;
        wxStaticBox* StaticBoxLayers;
        wxStaticBox* StaticBoxMaterial;
        wxStaticBox* StaticBoxParts;
        wxStaticBox* StaticBoxStatus;
        wxStaticBox* StaticBoxTool;
        wxStaticText* StaticTextConnTol;
        wxStaticText* StaticTextDepth;
        wxStaticText* StaticTextMatTopHeight;
        wxStaticText* StaticTextSafeZ;
        wxStaticText* StaticTextToolDiameter;
        wxStaticText* StaticTextToolFeedXY;
        wxStaticText* StaticTextToolFeedZ;
        wxStaticText* StaticTextToolOvercut;
        wxStaticText* StaticTextToolRPM;
        wxStaticText* StaticTextToolRampAngle;
        wxStaticText* StaticTextToolStepdown;
        wxStaticText* StaticTextToolStepover;
        wxTextCtrl* TextCtrlConnTolerance;
        wxTextCtrl* TextCtrlDepth;
        wxTextCtrl* TextCtrlMatTopHeight;
        wxTextCtrl* TextCtrlSafeZ;
        wxTextCtrl* TextCtrlStatus;
        wxTextCtrl* TextCtrlToolDiameter;
        wxTextCtrl* TextCtrlToolFeedXY;
        wxTextCtrl* TextCtrlToolFeedZ;
        wxTextCtrl* TextCtrlToolOvercut;
        wxTextCtrl* TextCtrlToolRPM;
        wxTextCtrl* TextCtrlToolRampAngle;
        wxTextCtrl* TextCtrlToolStepdown;
        wxTextCtrl* TextCtrlToolStepover;
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

        wxNotebook* NotebookRight = nullptr;
        wxPanel* TabPartDef = nullptr;
        wxPanel* TabMachining = nullptr;
        wxStaticBox* StaticBoxPartsMachining = nullptr;
        wxTreeCtrl* TreeCtrlPartsMachining = nullptr;

        bool m_isDirty = false;
        wxString m_loadedFileName;

        void UpdateWindowTitle();
        void MarkDirty();
        void MarkClean();
        void GenerateGCode(const wxString& filePath);

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
        void OnNotebookPageChanged(wxBookCtrlEvent& event);
        void OnTreeCtrlPartsMachiningSelChanged(wxTreeEvent& event);
        void LayoutPartDefTab();
        void LayoutMachiningTab();

        void UpdatePartButtonStates();
        void RebuildPartsTree();
        void RebuildPartsMachiningTree();
        void SyncPartDefTreeSelection();
        void SyncMachiningTreeSelection();
        void getMinMaxDepth(double& minDepth, double& maxDepth) const;
        wxColour getDepthColor(double depth, double minDepth, double maxDepth) const;
        std::vector<std::vector<DxfPoint>> getShapeClosedPaths(const CamShape& shape) const;
        std::vector<AnalyticalPath> getShapeAnalyticalPaths(const CamShape& shape) const;

        std::string m_currentDxfPath;
        std::string m_currentProjectPath;
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
        int m_selectedMachiningLevelIndex = -1;
        int m_nextPartId = 1;
        std::vector<ShapePaths> m_partShapePaths;
        SelectedShapeProcessingPaths m_procPaths;
        MachiningLevelPaths m_machPaths;
        LevelMachiningToolpaths m_levelMachiningPaths;

        DxfModel m_dxfModel;
        PHIDP_PREPARSED_DATA preparsedData = nullptr;

        DECLARE_EVENT_TABLE()
};

#endif // BENDICAMMAIN_H
