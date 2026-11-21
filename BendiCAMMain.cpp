/***************************************************************
 * Name:      BendiCAMMain.cpp
 * Purpose:   Code for Application Frame
 * Author:    David Bender (d_bender@bluewin.ch)
 * Created:   2026-02-12
 * Copyright: David Bender ()
 * License:
 **************************************************************/

#include "BendiCAMMain.h"
#include <wx/msgdlg.h>
#include <mmsystem.h>
#include <joystickapi.h>

//(*InternalHeaders(BendiCAMFrame)
#include <wx/intl.h>
#include <wx/string.h>
//*)

//helper functions
enum wxbuildinfoformat {
    short_f, long_f };

wxString wxbuildinfo(wxbuildinfoformat format)
{
    wxString wxbuild(wxVERSION_STRING);

    if (format == long_f )
    {
#if defined(__WXMSW__)
        wxbuild << _T("-Windows");
#elif defined(__UNIX__)
        wxbuild << _T("-Linux");
#endif

#if wxUSE_UNICODE
        wxbuild << _T("-Unicode build");
#else
        wxbuild << _T("-ANSI build");
#endif // wxUSE_UNICODE
    }

    return wxbuild;
}

static std::string escapeJson(const std::string& s) {
    std::string res;
    for (char c : s) {
        if (c == '\\') res += "\\\\";
        else if (c == '"') res += "\\\"";
        else if (c == '\n') res += "\\n";
        else if (c == '\r') res += "\\r";
        else if (c == '\t') res += "\\t";
        else res += c;
    }
    return res;
}

struct JsonValue {
    enum Type { NUL, BOOL, NUMBER, STRING, ARRAY, OBJECT } type = NUL;
    bool boolVal = false;
    double numVal = 0.0;
    std::string strVal;
    std::vector<JsonValue> arrVal;
    std::map<std::string, JsonValue> objVal;

    const JsonValue& operator[](const std::string& key) const {
        static JsonValue nullVal;
        if (type == OBJECT) {
            auto it = objVal.find(key);
            if (it != objVal.end()) return it->second;
        }
        return nullVal;
    }
    const JsonValue& operator[](size_t idx) const {
        static JsonValue nullVal;
        if (type == ARRAY && idx < arrVal.size()) return arrVal[idx];
        return nullVal;
    }
};

class JsonParser {
    std::string src;
    size_t pos = 0;

    void skipWhitespace() {
        while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\n' || src[pos] == '\r')) {
            pos++;
        }
    }

public:
    JsonValue parse(const std::string& jsonText) {
        src = jsonText;
        pos = 0;
        skipWhitespace();
        return parseValue();
    }

private:
    JsonValue parseValue() {
        skipWhitespace();
        if (pos >= src.size()) return JsonValue();
        char c = src[pos];
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') return parseString();
        if (c == 't' || c == 'f') return parseBool();
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
        return JsonValue();
    }

    JsonValue parseObject() {
        JsonValue val;
        val.type = JsonValue::OBJECT;
        pos++;
        skipWhitespace();
        if (pos < src.size() && src[pos] == '}') { pos++; return val; }
        while (pos < src.size()) {
            skipWhitespace();
            if (src[pos] != '"') break;
            JsonValue keyVal = parseString();
            skipWhitespace();
            if (pos < src.size() && src[pos] == ':') pos++;
            JsonValue childVal = parseValue();
            val.objVal[keyVal.strVal] = childVal;
            skipWhitespace();
            if (pos < src.size() && src[pos] == ',') pos++;
            else if (pos < src.size() && src[pos] == '}') { pos++; break; }
        }
        return val;
    }

    JsonValue parseArray() {
        JsonValue val;
        val.type = JsonValue::ARRAY;
        pos++;
        skipWhitespace();
        if (pos < src.size() && src[pos] == ']') { pos++; return val; }
        while (pos < src.size()) {
            val.arrVal.push_back(parseValue());
            skipWhitespace();
            if (pos < src.size() && src[pos] == ',') pos++;
            else if (pos < src.size() && src[pos] == ']') { pos++; break; }
        }
        return val;
    }

    JsonValue parseString() {
        JsonValue val;
        val.type = JsonValue::STRING;
        pos++;
        std::string s;
        while (pos < src.size()) {
            char c = src[pos++];
            if (c == '"') break;
            if (c == '\\' && pos < src.size()) {
                char esc = src[pos++];
                if (esc == '"') s += '"';
                else if (esc == '\\') s += '\\';
                else if (esc == 'n') s += '\n';
                else if (esc == 'r') s += '\r';
                else if (esc == 't') s += '\t';
                else s += esc;
            } else {
                s += c;
            }
        }
        val.strVal = s;
        return val;
    }

    JsonValue parseBool() {
        JsonValue val;
        val.type = JsonValue::BOOL;
        if (src.compare(pos, 4, "true") == 0) { val.boolVal = true; pos += 4; }
        else if (src.compare(pos, 5, "false") == 0) { val.boolVal = false; pos += 5; }
        return val;
    }

    JsonValue parseNumber() {
        JsonValue val;
        val.type = JsonValue::NUMBER;
        size_t start = pos;
        if (src[pos] == '-') pos++;
        while (pos < src.size() && ((src[pos] >= '0' && src[pos] <= '9') || src[pos] == '.' || src[pos] == 'e' || src[pos] == 'E' || src[pos] == '+' || src[pos] == '-')) {
            pos++;
        }
        std::string numStr = src.substr(start, pos - start);
        val.numVal = std::atof(numStr.c_str());
        return val;
    }
};

//(*IdInit(BendiCAMFrame)
const wxWindowID BendiCAMFrame::ID_PANEL1 = wxNewId();
const wxWindowID BendiCAMFrame::ID_PANEL2 = wxNewId();
const wxWindowID BendiCAMFrame::ID_STATICBOX3 = wxNewId();
const wxWindowID BendiCAMFrame::ID_CHECKLISTBOX1 = wxNewId();
const wxWindowID BendiCAMFrame::ID_STATICBOX4 = wxNewId();
const wxWindowID BendiCAMFrame::ID_TREECTRL1 = wxNewId();
const wxWindowID BendiCAMFrame::ID_STATICTEXT1 = wxNewId();
const wxWindowID BendiCAMFrame::ID_TEXTCTRL2 = wxNewId();
const wxWindowID BendiCAMFrame::ID_BUTTON1 = wxNewId();
const wxWindowID BendiCAMFrame::ID_BUTTON2 = wxNewId();
const wxWindowID BendiCAMFrame::ID_CHECKBOX1 = wxNewId();
const wxWindowID BendiCAMFrame::ID_STATICTEXT2 = wxNewId();
const wxWindowID BendiCAMFrame::ID_TEXTCTRL3 = wxNewId();
const wxWindowID BendiCAMFrame::ID_STATICBOX2 = wxNewId();
const wxWindowID BendiCAMFrame::ID_TEXTCTRL1 = wxNewId();
const wxWindowID BendiCAMFrame::ID_STATICBOX5 = wxNewId();
const wxWindowID BendiCAMFrame::ID_STATICTEXT3 = wxNewId();
const wxWindowID BendiCAMFrame::ID_TEXTCTRL4 = wxNewId();
const wxWindowID BendiCAMFrame::ID_STATICTEXT4 = wxNewId();
const wxWindowID BendiCAMFrame::ID_TEXTCTRL5 = wxNewId();
const wxWindowID BendiCAMFrame::ID_STATICTEXT5 = wxNewId();
const wxWindowID BendiCAMFrame::ID_TEXTCTRL6 = wxNewId();
const wxWindowID BendiCAMFrame::ID_STATICTEXT6 = wxNewId();
const wxWindowID BendiCAMFrame::ID_TEXTCTRL7 = wxNewId();
const wxWindowID BendiCAMFrame::ID_STATICTEXT7 = wxNewId();
const wxWindowID BendiCAMFrame::ID_TEXTCTRL8 = wxNewId();
const wxWindowID BendiCAMFrame::ID_STATICTEXT8 = wxNewId();
const wxWindowID BendiCAMFrame::ID_TEXTCTRL9 = wxNewId();
const wxWindowID BendiCAMFrame::ID_STATICTEXT9 = wxNewId();
const wxWindowID BendiCAMFrame::ID_TEXTCTRL10 = wxNewId();
const wxWindowID BendiCAMFrame::ID_STATICBOX6 = wxNewId();
const wxWindowID BendiCAMFrame::ID_STATICTEXT10 = wxNewId();
const wxWindowID BendiCAMFrame::ID_TEXTCTRL11 = wxNewId();
const wxWindowID BendiCAMFrame::ID_STATICTEXT11 = wxNewId();
const wxWindowID BendiCAMFrame::ID_TEXTCTRL12 = wxNewId();
const wxWindowID BendiCAMFrame::ID_BUTTON3 = wxNewId();
const wxWindowID BendiCAMFrame::idMenuOpen = wxNewId();
const wxWindowID BendiCAMFrame::idMenuOpenProject = wxNewId();
const wxWindowID BendiCAMFrame::idMenuSaveProject = wxNewId();
const wxWindowID BendiCAMFrame::ID_MENUITEM1 = wxNewId();
const wxWindowID BendiCAMFrame::idMenuAbout = wxNewId();
const wxWindowID BendiCAMFrame::ID_TIMER = wxNewId();
//*)

BEGIN_EVENT_TABLE(BendiCAMFrame,wxFrame)
    //(*EventTable(BendiCAMFrame)
    //*)
END_EVENT_TABLE()

BendiCAMFrame::BendiCAMFrame(wxWindow* parent,wxWindowID id)
{
    //(*Initialize(BendiCAMFrame)
    wxMenu* Menu1;
    wxMenu* Menu2;
    wxMenuBar* MenuBar1;
    wxMenuItem* MenuItem1;
    wxMenuItem* MenuItem2;
    wxMenuItem* MenuItemOpen;
    wxMenuItem* MenuItemOpenProject;
    wxMenuItem* MenuItemSaveProject;

    Create(parent, wxID_ANY, _("BendiCAM"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE|wxBORDER_DOUBLE, _T("wxID_ANY"));
    SetClientSize(wxSize(927,578));
    PanelDrawing = new wxPanel(this, ID_PANEL1, wxPoint(24,24), wxSize(320,360), wxBORDER_SUNKEN|wxTAB_TRAVERSAL|wxVSCROLL|wxHSCROLL, _T("ID_PANEL1"));
    PanelDrawing->SetBackgroundColour(wxColour(255,255,255));
    Panel3D = new wxPanel(this, ID_PANEL2, wxPoint(352,24), wxSize(316,360), wxBORDER_SUNKEN|wxTAB_TRAVERSAL, _T("ID_PANEL2"));
    Panel3D->SetBackgroundColour(wxColour(245,245,250));
    StaticBoxLayers = new wxStaticBox(this, ID_STATICBOX3, _("Layers"), wxPoint(680,24), wxSize(223,170), 0, _T("ID_STATICBOX3"));
    CheckListBoxLayers = new wxCheckListBox(this, ID_CHECKLISTBOX1, wxPoint(692,44), wxSize(199,140), 0, 0, 0, wxDefaultValidator, _T("ID_CHECKLISTBOX1"));
    StaticBoxParts = new wxStaticBox(this, ID_STATICBOX4, _("Parts"), wxPoint(680,195), wxSize(223,195), 0, _T("ID_STATICBOX4"));
    TreeCtrlParts = new wxTreeCtrl(this, ID_TREECTRL1, wxPoint(692,212), wxSize(199,48), wxTR_HAS_BUTTONS|wxTR_LINES_AT_ROOT|wxTR_HIDE_ROOT|wxTR_SINGLE, wxDefaultValidator, _T("ID_TREECTRL1"));
    StaticTextDepth = new wxStaticText(this, ID_STATICTEXT1, _("Depth (mm):"), wxPoint(692,266), wxDefaultSize, 0, _T("ID_STATICTEXT1"));
    TextCtrlDepth = new wxTextCtrl(this, ID_TEXTCTRL2, _T("5.0"), wxPoint(768,264), wxSize(123,22), 0, wxDefaultValidator, _T("ID_TEXTCTRL2"));
    ButtonCreatePart = new wxButton(this, ID_BUTTON1, _("Create part"), wxPoint(692,290), wxSize(96,24), 0, wxDefaultValidator, _T("ID_BUTTON1"));
    ButtonAddToPart = new wxButton(this, ID_BUTTON2, _("Add to part"), wxPoint(795,290), wxSize(96,24), 0, wxDefaultValidator, _T("ID_BUTTON2"));
    CheckBoxClosePolylines = new wxCheckBox(this, ID_CHECKBOX1, _("Close Matching Endpoints"), wxPoint(692,318), wxSize(205,34), 0, wxDefaultValidator, _T("ID_CHECKBOX1"));
    CheckBoxClosePolylines->SetValue(true);
    StaticTextConnTol = new wxStaticText(this, ID_STATICTEXT2, _("Matching Tolerance:"), wxPoint(692,358), wxDefaultSize, 0, _T("ID_STATICTEXT2"));
    TextCtrlConnTolerance = new wxTextCtrl(this, ID_TEXTCTRL3, _T("0.1"), wxPoint(825,356), wxSize(66,22), 0, wxDefaultValidator, _T("ID_TEXTCTRL3"));
    StaticBoxStatus = new wxStaticBox(this, ID_STATICBOX2, _("Status"), wxPoint(24,400), wxSize(879,136), 0, _T("ID_STATICBOX2"));
    TextCtrlStatus = new wxTextCtrl(this, ID_TEXTCTRL1, _("Running\n"), wxPoint(40,424), wxSize(847,96), wxTE_MULTILINE|wxVSCROLL, wxDefaultValidator, _T("ID_TEXTCTRL1"));
    StaticBoxTool = new wxStaticBox(this, ID_STATICBOX5, _("Tool (Milling Cutter)"), wxPoint(680,396), wxSize(223,135), 0, _T("ID_STATICBOX5"));
    StaticTextToolDiameter = new wxStaticText(this, ID_STATICTEXT3, _("Diameter (mm):"), wxPoint(692,416), wxDefaultSize, 0, _T("ID_STATICTEXT3"));
    TextCtrlToolDiameter = new wxTextCtrl(this, ID_TEXTCTRL4, _T("2"), wxPoint(768,414), wxSize(40,22), wxTE_RIGHT, wxDefaultValidator, _T("ID_TEXTCTRL4"));
    StaticTextToolRPM = new wxStaticText(this, ID_STATICTEXT4, _("RPM (min^-1):"), wxPoint(815,416), wxDefaultSize, 0, _T("ID_STATICTEXT4"));
    TextCtrlToolRPM = new wxTextCtrl(this, ID_TEXTCTRL5, _T("12000"), wxPoint(880,414), wxSize(40,22), wxTE_RIGHT, wxDefaultValidator, _T("ID_TEXTCTRL5"));
    StaticTextToolFeedXY = new wxStaticText(this, ID_STATICTEXT5, _("Feed XY (mm/min):"), wxPoint(692,442), wxDefaultSize, 0, _T("ID_STATICTEXT5"));
    TextCtrlToolFeedXY = new wxTextCtrl(this, ID_TEXTCTRL6, _T("500"), wxPoint(768,440), wxSize(40,22), wxTE_RIGHT, wxDefaultValidator, _T("ID_TEXTCTRL6"));
    StaticTextToolFeedZ = new wxStaticText(this, ID_STATICTEXT6, _("Feed Z (mm/min):"), wxPoint(815,442), wxDefaultSize, 0, _T("ID_STATICTEXT6"));
    TextCtrlToolFeedZ = new wxTextCtrl(this, ID_TEXTCTRL7, _T("250"), wxPoint(880,440), wxSize(40,22), wxTE_RIGHT, wxDefaultValidator, _T("ID_TEXTCTRL7"));
    StaticTextToolStepdown = new wxStaticText(this, ID_STATICTEXT7, _("Stepdown (mm):"), wxPoint(692,468), wxDefaultSize, 0, _T("ID_STATICTEXT7"));
    TextCtrlToolStepdown = new wxTextCtrl(this, ID_TEXTCTRL8, _T("1"), wxPoint(768,466), wxSize(40,22), wxTE_RIGHT, wxDefaultValidator, _T("ID_TEXTCTRL8"));
    StaticTextToolStepover = new wxStaticText(this, ID_STATICTEXT8, _("Stepover (%):"), wxPoint(815,468), wxDefaultSize, 0, _T("ID_STATICTEXT8"));
    TextCtrlToolStepover = new wxTextCtrl(this, ID_TEXTCTRL9, _T("70"), wxPoint(880,466), wxSize(40,22), wxTE_RIGHT, wxDefaultValidator, _T("ID_TEXTCTRL9"));
    StaticTextToolRampAngle = new wxStaticText(this, ID_STATICTEXT9, _("Ramp Angle (�):"), wxPoint(692,494), wxDefaultSize, 0, _T("ID_STATICTEXT9"));
    TextCtrlToolRampAngle = new wxTextCtrl(this, ID_TEXTCTRL10, _T("20"), wxPoint(768,492), wxSize(40,22), wxTE_RIGHT, wxDefaultValidator, _T("ID_TEXTCTRL10"));
    StaticBoxMaterial = new wxStaticBox(this, ID_STATICBOX6, _("Material & Safety"), wxPoint(680,537), wxSize(223,50), 0, _T("ID_STATICBOX6"));
    StaticTextMatTopHeight = new wxStaticText(this, ID_STATICTEXT10, _("Top Height (mm):"), wxPoint(692,555), wxDefaultSize, 0, _T("ID_STATICTEXT10"));
    TextCtrlMatTopHeight = new wxTextCtrl(this, ID_TEXTCTRL11, _T("10"), wxPoint(768,553), wxSize(40,22), wxTE_RIGHT, wxDefaultValidator, _T("ID_TEXTCTRL11"));
    StaticTextSafeZ = new wxStaticText(this, ID_STATICTEXT11, _("Safe Z (mm):"), wxPoint(815,555), wxDefaultSize, 0, _T("ID_STATICTEXT11"));
    TextCtrlSafeZ = new wxTextCtrl(this, ID_TEXTCTRL12, _T("10"), wxPoint(880,553), wxSize(40,22), wxTE_RIGHT, wxDefaultValidator, _T("ID_TEXTCTRL12"));
    ButtonExportGCode = new wxButton(this, ID_BUTTON3, _("Export EdingCNC G-Code..."), wxPoint(680,595), wxSize(223,28), 0, wxDefaultValidator, _T("ID_BUTTON3"));
    MenuBar1 = new wxMenuBar();
    Menu1 = new wxMenu();
    MenuItemOpen = new wxMenuItem(Menu1, idMenuOpen, _("Open CAD Drawing\tCtrl-O"), _("Open a CAD drawing file"), wxITEM_NORMAL);
    Menu1->Append(MenuItemOpen);
    MenuItemOpenProject = new wxMenuItem(Menu1, idMenuOpenProject, _("Open BendiCAM Project\tCtrl-P"), _("Open a BendiCAM project file"), wxITEM_NORMAL);
    Menu1->Append(MenuItemOpenProject);
    MenuItemSaveProject = new wxMenuItem(Menu1, idMenuSaveProject, _("Save BendiCAM Project\tCtrl-S"), _("Save current project to file"), wxITEM_NORMAL);
    Menu1->Append(MenuItemSaveProject);
    MenuItem1 = new wxMenuItem(Menu1, ID_MENUITEM1, _("Quit\tAlt-F4"), _("Quit the application"), wxITEM_NORMAL);
    Menu1->Append(MenuItem1);
    MenuBar1->Append(Menu1, _("&File"));
    Menu2 = new wxMenu();
    MenuItem2 = new wxMenuItem(Menu2, idMenuAbout, _("About\tF1"), _("Show info about this application"), wxITEM_NORMAL);
    Menu2->Append(MenuItem2);
    MenuBar1->Append(Menu2, _("Help"));
    SetMenuBar(MenuBar1);
    Timer.SetOwner(this, ID_TIMER);
    Timer.Start(200, false);

    Connect(ID_CHECKLISTBOX1, wxEVT_COMMAND_CHECKLISTBOX_TOGGLED, (wxObjectEventFunction)&BendiCAMFrame::OnCheckListBoxLayersToggled);
    Connect(ID_TREECTRL1, wxEVT_COMMAND_TREE_SEL_CHANGED, (wxObjectEventFunction)&BendiCAMFrame::OnTreeCtrlPartsSelChanged);
    Connect(ID_BUTTON1, wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&BendiCAMFrame::OnButtonCreatePartClick);
    Connect(ID_BUTTON2, wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&BendiCAMFrame::OnButtonAddToPartClick);
    Connect(ID_CHECKBOX1, wxEVT_COMMAND_CHECKBOX_CLICKED, (wxObjectEventFunction)&BendiCAMFrame::OnCheckBoxClosePolylinesClick);
    Connect(ID_TEXTCTRL4, wxEVT_COMMAND_TEXT_UPDATED, (wxObjectEventFunction)&BendiCAMFrame::OnToolOrMaterialSettingChange);
    Connect(ID_TEXTCTRL5, wxEVT_COMMAND_TEXT_UPDATED, (wxObjectEventFunction)&BendiCAMFrame::OnToolOrMaterialSettingChange);
    Connect(ID_TEXTCTRL6, wxEVT_COMMAND_TEXT_UPDATED, (wxObjectEventFunction)&BendiCAMFrame::OnToolOrMaterialSettingChange);
    Connect(ID_TEXTCTRL7, wxEVT_COMMAND_TEXT_UPDATED, (wxObjectEventFunction)&BendiCAMFrame::OnToolOrMaterialSettingChange);
    Connect(ID_TEXTCTRL8, wxEVT_COMMAND_TEXT_UPDATED, (wxObjectEventFunction)&BendiCAMFrame::OnToolOrMaterialSettingChange);
    Connect(ID_TEXTCTRL9, wxEVT_COMMAND_TEXT_UPDATED, (wxObjectEventFunction)&BendiCAMFrame::OnToolOrMaterialSettingChange);
    Connect(ID_TEXTCTRL10, wxEVT_COMMAND_TEXT_UPDATED, (wxObjectEventFunction)&BendiCAMFrame::OnToolOrMaterialSettingChange);
    Connect(ID_TEXTCTRL11, wxEVT_COMMAND_TEXT_UPDATED, (wxObjectEventFunction)&BendiCAMFrame::OnToolOrMaterialSettingChange);
    Connect(ID_TEXTCTRL12, wxEVT_COMMAND_TEXT_UPDATED, (wxObjectEventFunction)&BendiCAMFrame::OnToolOrMaterialSettingChange);
    Connect(ID_BUTTON3, wxEVT_COMMAND_BUTTON_CLICKED, (wxObjectEventFunction)&BendiCAMFrame::OnExportGCode);
    Connect(idMenuOpen, wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&BendiCAMFrame::OnOpen);
    Connect(idMenuOpenProject, wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&BendiCAMFrame::OnOpenProject);
    Connect(idMenuSaveProject, wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&BendiCAMFrame::OnSaveProject);
    Connect(ID_MENUITEM1, wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&BendiCAMFrame::OnQuit);
    Connect(idMenuAbout, wxEVT_COMMAND_MENU_SELECTED, (wxObjectEventFunction)&BendiCAMFrame::OnAbout);
    Connect(ID_TIMER, wxEVT_TIMER, (wxObjectEventFunction)&BendiCAMFrame::OnTimerTrigger);
    Connect(wxID_ANY, wxEVT_CLOSE_WINDOW, (wxObjectEventFunction)&BendiCAMFrame::OnClose);
    //*)

    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));

    // Configure Splitter Hierarchy and Parent Relationships
    wxBoxSizer* frameSizer = new wxBoxSizer(wxVERTICAL);

    SplitterMain = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE | wxSP_3D);
    SplitterMain->SetMinimumPaneSize(50);
    SplitterMain->SetSashGravity(1.0);

    PanelTop = new wxPanel(SplitterMain, wxID_ANY);
    PanelBottom = new wxPanel(SplitterMain, wxID_ANY);

    frameSizer->Add(SplitterMain, 1, wxEXPAND);
    SetSizer(frameSizer);

    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
    SplitterTop = new wxSplitterWindow(PanelTop, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE | wxSP_3D);
    SplitterTop->SetMinimumPaneSize(100);
    SplitterTop->SetSashGravity(1.0);

    PanelDrawAnd3D = new wxPanel(SplitterTop, wxID_ANY);
    PanelRight = new wxPanel(SplitterTop, wxID_ANY);

    topSizer->Add(SplitterTop, 1, wxEXPAND);
    PanelTop->SetSizer(topSizer);

    wxBoxSizer* draw3dSizer = new wxBoxSizer(wxVERTICAL);
    SplitterDraw3D = new wxSplitterWindow(PanelDrawAnd3D, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE | wxSP_3D);
    SplitterDraw3D->SetMinimumPaneSize(50);
    SplitterDraw3D->SetSashGravity(0.65);

    draw3dSizer->Add(SplitterDraw3D, 1, wxEXPAND);
    PanelDrawAnd3D->SetSizer(draw3dSizer);

    // Re-parent Panels & Groupboxes
    PanelDrawing->Reparent(SplitterDraw3D);
    Panel3D->Reparent(SplitterDraw3D);

    StaticBoxLayers->Reparent(PanelRight);
    CheckListBoxLayers->Reparent(PanelRight);

    StaticBoxParts->Reparent(PanelRight);
    TreeCtrlParts->Reparent(PanelRight);
    StaticTextDepth->Reparent(PanelRight);
    TextCtrlDepth->Reparent(PanelRight);
    ButtonCreatePart->Reparent(PanelRight);
    ButtonAddToPart->Reparent(PanelRight);
    CheckBoxClosePolylines->Reparent(PanelRight);
    StaticTextConnTol->Reparent(PanelRight);
    TextCtrlConnTolerance->Reparent(PanelRight);

    StaticBoxStatus->Reparent(PanelBottom);
    TextCtrlStatus->Reparent(PanelBottom);

    StaticBoxTool->Reparent(PanelRight);
    StaticTextToolDiameter->Reparent(PanelRight);
    TextCtrlToolDiameter->Reparent(PanelRight);
    StaticTextToolRPM->Reparent(PanelRight);
    TextCtrlToolRPM->Reparent(PanelRight);
    StaticTextToolFeedXY->Reparent(PanelRight);
    TextCtrlToolFeedXY->Reparent(PanelRight);
    StaticTextToolFeedZ->Reparent(PanelRight);
    TextCtrlToolFeedZ->Reparent(PanelRight);
    StaticTextToolStepdown->Reparent(PanelRight);
    TextCtrlToolStepdown->Reparent(PanelRight);
    StaticTextToolStepover->Reparent(PanelRight);
    TextCtrlToolStepover->Reparent(PanelRight);
    StaticTextToolRampAngle->Reparent(PanelRight);
    TextCtrlToolRampAngle->Reparent(PanelRight);

    StaticBoxMaterial->Reparent(PanelRight);
    StaticTextMatTopHeight->Reparent(PanelRight);
    TextCtrlMatTopHeight->Reparent(PanelRight);
    StaticTextSafeZ->Reparent(PanelRight);
    TextCtrlSafeZ->Reparent(PanelRight);

    ButtonExportGCode->Reparent(PanelRight);

    SplitterDraw3D->SplitVertically(PanelDrawing, Panel3D);
    SplitterTop->SplitVertically(PanelDrawAnd3D, PanelRight);
    SplitterMain->SplitHorizontally(PanelTop, PanelBottom);

    SplitterMain->SetSashPosition(-115);
    SplitterTop->SetSashPosition(-300);

    UpdateWindowTitle();

    PanelRight->Bind(wxEVT_SIZE, &BendiCAMFrame::OnRightPanelSize, this);
    PanelBottom->Bind(wxEVT_SIZE, &BendiCAMFrame::OnBottomPanelSize, this);
    PanelDrawing->SetBackgroundStyle(wxBG_STYLE_PAINT);
    PanelDrawing->Bind(wxEVT_PAINT, &BendiCAMFrame::OnPanelPaint, this);
    PanelDrawing->Bind(wxEVT_ERASE_BACKGROUND, &BendiCAMFrame::OnPanelEraseBg, this);
    PanelDrawing->Bind(wxEVT_MOUSEWHEEL, &BendiCAMFrame::OnPanelMouseWheel, this);
    PanelDrawing->Bind(wxEVT_MIDDLE_DOWN, &BendiCAMFrame::OnPanelMiddleDown, this);
    PanelDrawing->Bind(wxEVT_MIDDLE_UP, &BendiCAMFrame::OnPanelMiddleUp, this);
    PanelDrawing->Bind(wxEVT_MOTION, &BendiCAMFrame::OnPanelMouseMove, this);
    PanelDrawing->Bind(wxEVT_LEFT_DOWN, &BendiCAMFrame::OnPanelLeftDown, this);

    Panel3D->SetBackgroundStyle(wxBG_STYLE_PAINT);
    Panel3D->Bind(wxEVT_PAINT, &BendiCAMFrame::OnPanel3DPaint, this);
    Panel3D->Bind(wxEVT_ERASE_BACKGROUND, &BendiCAMFrame::OnPanel3DEraseBg, this);
    Panel3D->Bind(wxEVT_LEFT_DOWN, &BendiCAMFrame::OnPanel3DLeftDown, this);
    Panel3D->Bind(wxEVT_LEFT_UP, &BendiCAMFrame::OnPanel3DLeftUp, this);
    Panel3D->Bind(wxEVT_MIDDLE_DOWN, &BendiCAMFrame::OnPanel3DMiddleDown, this);
    Panel3D->Bind(wxEVT_MIDDLE_UP, &BendiCAMFrame::OnPanel3DMiddleUp, this);
    Panel3D->Bind(wxEVT_MOTION, &BendiCAMFrame::OnPanel3DMouseMove, this);
    Panel3D->Bind(wxEVT_MOUSEWHEEL, &BendiCAMFrame::OnPanel3DMouseWheel, this);

    preparsedData = nullptr;

    UpdatePartButtonStates();

    Bind(wxEVT_SIZE, &BendiCAMFrame::OnSize, this);
    wxSizeEvent dummyEvt;
    OnSize(dummyEvt);
}

BendiCAMFrame::~BendiCAMFrame()
{
    //(*Destroy(BendiCAMFrame)
    //*)
    if (preparsedData != nullptr) {
        free(preparsedData);
    }
}

void BendiCAMFrame::OnAbout(wxCommandEvent& event)
{
    wxString msg = wxbuildinfo(long_f);
    wxMessageBox(msg, _("Welcome to..."));
}

void BendiCAMFrame::OnSize(wxSizeEvent& event)
{
    event.Skip();
    Layout();
    if (PanelDrawAnd3D && SplitterDraw3D) {
        int totalW = PanelDrawAnd3D->GetClientSize().GetWidth();
        if (totalW > 100) {
            SplitterDraw3D->SetSashPosition((int)(totalW * 0.65));
        }
    }
}

void BendiCAMFrame::OnRightPanelSize(wxSizeEvent& event)
{
    event.Skip();
    if (!PanelRight) return;
    int rw = PanelRight->GetClientSize().GetWidth();
    int rh = PanelRight->GetClientSize().GetHeight();
    if (rw <= 20 || rh <= 20) return;

    int layersH = 110;
    if (StaticBoxLayers) {
        StaticBoxLayers->SetSize(4, 4, rw - 8, layersH);
        StaticBoxLayers->Lower();
        if (CheckListBoxLayers) CheckListBoxLayers->SetSize(14, 24, rw - 28, layersH - 28);
    }

    int partsY = layersH + 6;
    int partsH = 155;
    if (StaticBoxParts) {
        StaticBoxParts->SetSize(4, partsY, rw - 8, partsH);
        StaticBoxParts->Lower();

        int boxW = rw - 8;
        int bottomPadding = 6;
        int rowConnTolY = partsY + partsH - bottomPadding - 22;
        if (StaticTextConnTol) StaticTextConnTol->SetSize(14, rowConnTolY + 2, 120, 20);
        if (TextCtrlConnTolerance) TextCtrlConnTolerance->SetSize(136, rowConnTolY, 4 + boxW - 136 - 10, 22);

        int rowCheckY = rowConnTolY - 24;
        if (CheckBoxClosePolylines) CheckBoxClosePolylines->SetSize(14, rowCheckY, boxW - 20, 22);

        int rowBtnY = rowCheckY - 26;
        int btnW = (boxW - 20 - 8) / 2;
        if (ButtonCreatePart) ButtonCreatePart->SetSize(14, rowBtnY, btnW, 24);
        if (ButtonAddToPart) ButtonAddToPart->SetSize(14 + btnW + 8, rowBtnY, btnW, 24);

        int rowDepthY = rowBtnY - 24;
        if (StaticTextDepth) StaticTextDepth->SetSize(14, rowDepthY + 2, 70, 20);
        if (TextCtrlDepth) TextCtrlDepth->SetSize(86, rowDepthY, 4 + boxW - 86 - 10, 22);

        int treeTopY = partsY + 18;
        int treeH = std::max(20, rowDepthY - 4 - treeTopY);
        if (TreeCtrlParts) TreeCtrlParts->SetSize(14, treeTopY, boxW - 20, treeH);
    }

    int toolY = partsY + partsH + 6;
    int toolH = 135;
    if (StaticBoxTool) {
        StaticBoxTool->SetSize(4, toolY, rw - 8, toolH);
        StaticBoxTool->Lower();

        int boxW = rw - 8;
        int midX = 4 + boxW / 2;
        int leftLabelW = 112;
        int rightLabelW = 112;

        // Row 1: Diameter (mm) | RPM (min^-1)
        int r1Y = toolY + 18;
        if (StaticTextToolDiameter) StaticTextToolDiameter->SetSize(14, r1Y + 2, leftLabelW, 20);
        if (TextCtrlToolDiameter) TextCtrlToolDiameter->SetSize(14 + leftLabelW, r1Y, std::max(10, midX - (14 + leftLabelW) - 4), 22);

        if (StaticTextToolRPM) StaticTextToolRPM->SetSize(midX + 4, r1Y + 2, rightLabelW, 20);
        if (TextCtrlToolRPM) TextCtrlToolRPM->SetSize(midX + 4 + rightLabelW, r1Y, std::max(10, (4 + boxW - 8) - (midX + 4 + rightLabelW)), 22);

        // Row 2: Feed XY (mm/min) | Feed Z (mm/min)
        int r2Y = toolY + 44;
        if (StaticTextToolFeedXY) StaticTextToolFeedXY->SetSize(14, r2Y + 2, leftLabelW, 20);
        if (TextCtrlToolFeedXY) TextCtrlToolFeedXY->SetSize(14 + leftLabelW, r2Y, std::max(10, midX - (14 + leftLabelW) - 4), 22);

        if (StaticTextToolFeedZ) StaticTextToolFeedZ->SetSize(midX + 4, r2Y + 2, rightLabelW, 20);
        if (TextCtrlToolFeedZ) TextCtrlToolFeedZ->SetSize(midX + 4 + rightLabelW, r2Y, std::max(10, (4 + boxW - 8) - (midX + 4 + rightLabelW)), 22);

        // Row 3: Stepdown (mm) | Stepover (%)
        int r3Y = toolY + 70;
        if (StaticTextToolStepdown) StaticTextToolStepdown->SetSize(14, r3Y + 2, leftLabelW, 20);
        if (TextCtrlToolStepdown) TextCtrlToolStepdown->SetSize(14 + leftLabelW, r3Y, std::max(10, midX - (14 + leftLabelW) - 4), 22);

        if (StaticTextToolStepover) StaticTextToolStepover->SetSize(midX + 4, r3Y + 2, rightLabelW, 20);
        if (TextCtrlToolStepover) TextCtrlToolStepover->SetSize(midX + 4 + rightLabelW, r3Y, std::max(10, (4 + boxW - 8) - (midX + 4 + rightLabelW)), 22);

        // Row 4: Ramp Angle
        int r4Y = toolY + 96;
        if (StaticTextToolRampAngle) StaticTextToolRampAngle->SetSize(14, r4Y + 2, leftLabelW, 20);
        if (TextCtrlToolRampAngle) TextCtrlToolRampAngle->SetSize(14 + leftLabelW, r4Y, std::max(10, midX - (14 + leftLabelW) - 4), 22);
    }

    int matY = toolY + toolH + 6;
    int matH = 50;
    if (StaticBoxMaterial) {
        StaticBoxMaterial->SetSize(4, matY, rw - 8, matH);
        StaticBoxMaterial->Lower();

        int boxW = rw - 8;
        int midX = 4 + boxW / 2;
        int leftLabelW = 112;
        int rightLabelW = 112;
        int r1Y = matY + 18;

        if (StaticTextMatTopHeight) StaticTextMatTopHeight->SetSize(14, r1Y + 2, leftLabelW, 20);
        if (TextCtrlMatTopHeight) TextCtrlMatTopHeight->SetSize(14 + leftLabelW, r1Y, std::max(10, midX - (14 + leftLabelW) - 4), 22);

        if (StaticTextSafeZ) StaticTextSafeZ->SetSize(midX + 4, r1Y + 2, rightLabelW, 20);
        if (TextCtrlSafeZ) TextCtrlSafeZ->SetSize(midX + 4 + rightLabelW, r1Y, std::max(10, (4 + boxW - 8) - (midX + 4 + rightLabelW)), 22);
    }

    int btnY = matY + matH + 8;
    if (ButtonExportGCode) {
        ButtonExportGCode->SetSize(4, btnY, rw - 8, 28);
    }
}

void BendiCAMFrame::OnBottomPanelSize(wxSizeEvent& event)
{
    event.Skip();
    if (!PanelBottom) return;
    int bw = PanelBottom->GetClientSize().GetWidth();
    int bh = PanelBottom->GetClientSize().GetHeight();
    if (bw <= 20 || bh <= 20) return;

    if (StaticBoxStatus) {
        StaticBoxStatus->SetSize(4, 4, bw - 8, bh - 8);
        StaticBoxStatus->Lower();
        if (TextCtrlStatus) TextCtrlStatus->SetSize(14, 24, bw - 28, bh - 28);
    }
}

void BendiCAMFrame::getMinMaxDepth(double& minDepth, double& maxDepth) const
{
    minDepth = 1e9;
    maxDepth = -1e9;
    bool found = false;
    for (const auto& part : m_parts) {
        for (const auto& shape : part.shapes) {
            if (shape.depth < minDepth) minDepth = shape.depth;
            if (shape.depth > maxDepth) maxDepth = shape.depth;
            found = true;
        }
    }
    if (!found) {
        minDepth = maxDepth = 0.0;
    }
}

wxColour BendiCAMFrame::getDepthColor(double depth, double minDepth, double maxDepth) const
{
    if (maxDepth <= minDepth) {
        return wxColour(0, 220, 255); // Default Cyan glow
    }
    double t = (depth - minDepth) / (maxDepth - minDepth);
    t = std::max(0.0, std::min(1.0, t));

    unsigned char r, g, b;
    if (t < 0.5) {
        double factor = t / 0.5;
        r = (unsigned char)(0 + factor * 255);
        g = (unsigned char)(220 + factor * 0);
        b = (unsigned char)(255 * (1.0 - factor));
    } else {
        double factor = (t - 0.5) / 0.5;
        r = 255;
        g = (unsigned char)(220 * (1.0 - factor));
        b = 0;
    }
    return wxColour(r, g, b);
}

void BendiCAMFrame::RebuildPartsTree()
{
    static bool s_isRebuilding = false;
    if (s_isRebuilding) return;
    s_isRebuilding = true;

    TreeCtrlParts->DeleteAllItems();
    wxTreeItemId rootId = TreeCtrlParts->AddRoot(_("Parts"));

    for (int pIdx = 0; pIdx < (int)m_parts.size(); ++pIdx) {
        const auto& part = m_parts[pIdx];
        wxTreeItemId partId = TreeCtrlParts->AppendItem(rootId, wxString::FromUTF8(part.name.c_str()), -1, -1, new PartTreeItemData(pIdx, -1));

        for (int sIdx = 0; sIdx < (int)part.shapes.size(); ++sIdx) {
            const auto& shape = part.shapes[sIdx];
            wxString shapeLabel = wxString::Format(_("Shape %d (Depth: %.1f mm)"), sIdx + 1, shape.depth);
            TreeCtrlParts->AppendItem(partId, shapeLabel, -1, -1, new PartTreeItemData(pIdx, sIdx));
        }

        TreeCtrlParts->Expand(partId);
    }

    s_isRebuilding = false;
}

void BendiCAMFrame::OnTreeCtrlPartsSelChanged(wxTreeEvent& event)
{
    wxTreeItemId item = event.GetItem();
    if (item.IsOk()) {
        PartTreeItemData* data = (PartTreeItemData*)TreeCtrlParts->GetItemData(item);
        if (data) {
            m_selectedPartIndex = data->partIndex;
            m_selectedShapeInPartIndex = data->shapeIndex;

            if (m_selectedPartIndex >= 0 && m_selectedPartIndex < (int)m_parts.size()) {
                if (m_selectedShapeInPartIndex >= 0 && m_selectedShapeInPartIndex < (int)m_parts[m_selectedPartIndex].shapes.size()) {
                    double d = m_parts[m_selectedPartIndex].shapes[m_selectedShapeInPartIndex].depth;
                    TextCtrlDepth->ChangeValue(wxString::Format(_("%.1f"), d));
                } else if (!m_parts[m_selectedPartIndex].shapes.empty()) {
                    double d = m_parts[m_selectedPartIndex].shapes[0].depth;
                    TextCtrlDepth->ChangeValue(wxString::Format(_("%.1f"), d));
                }
            }
        } else {
            m_selectedPartIndex = -1;
            m_selectedShapeInPartIndex = -1;
        }
    } else {
        m_selectedPartIndex = -1;
        m_selectedShapeInPartIndex = -1;
    }

    UpdatePartButtonStates();
    PanelDrawing->Refresh();
    Panel3D->Refresh();
}

void BendiCAMFrame::OnTextCtrlDepthChange(wxCommandEvent& event)
{
    if (m_selectedPartIndex < 0 || m_selectedPartIndex >= (int)m_parts.size())
        return;

    wxString depthStr = TextCtrlDepth->GetValue();
    double newDepth = 5.0;
    if (!depthStr.ToDouble(&newDepth)) return;

    if (m_selectedShapeInPartIndex >= 0 && m_selectedShapeInPartIndex < (int)m_parts[m_selectedPartIndex].shapes.size()) {
        m_parts[m_selectedPartIndex].shapes[m_selectedShapeInPartIndex].depth = newDepth;

        wxTreeItemId selectedItem = TreeCtrlParts->GetSelection();
        if (selectedItem.IsOk()) {
            wxString shapeLabel = wxString::Format(_("Shape %d (Depth: %.1f mm)"), m_selectedShapeInPartIndex + 1, newDepth);
            TreeCtrlParts->SetItemText(selectedItem, shapeLabel);
        }
        PanelDrawing->Refresh();
        Panel3D->Refresh();
        MarkDirty();
    }
}

void BendiCAMFrame::OnCheckBoxClosePolylinesClick(wxCommandEvent& event)
{
    bool isChecked = CheckBoxClosePolylines->IsChecked();
    TextCtrlStatus->AppendText(wxString::Format(_("Close matching endpoints: %s\n"), isChecked ? _("Enabled") : _("Disabled")));
    MarkDirty();
}

void BendiCAMFrame::UpdatePartButtonStates()
{
    bool hasShape = !m_selectedIndices.empty();
    bool hasPart = (m_selectedPartIndex >= 0 && m_selectedPartIndex < (int)m_parts.size());

    ButtonCreatePart->Enable(hasShape && !hasPart);
    ButtonAddToPart->Enable(hasShape && hasPart);
}

void BendiCAMFrame::OnButtonCreatePartClick(wxCommandEvent& event)
{
    if (m_selectedIndices.empty())
        return;

    double depthVal = 5.0;
    wxString depthStr = TextCtrlDepth->GetValue();
    depthStr.ToDouble(&depthVal);

    CamPart part;
    part.id = m_nextPartId++;

    CamShape shape;
    shape.shapeId = 1;
    shape.depth = depthVal;
    shape.entityIndices = m_selectedIndices;

    part.shapes.push_back(shape);
    part.name = wxString::Format(_("Part %d (%zu shape(s))"), part.id, part.shapes.size()).ToStdString();

    m_parts.push_back(part);

    m_selectedIndices.clear();
    RebuildPartsTree();

    m_selectedPartIndex = (int)m_parts.size() - 1;
    m_selectedShapeInPartIndex = -1;
    UpdatePartButtonStates();

    TextCtrlStatus->AppendText(wxString::Format(_("Created Part %d (Depth %.1f mm) with %zu element(s)\n"),
                                               part.id, depthVal, shape.entityIndices.size()));
    PanelDrawing->Refresh();
    Panel3D->Refresh();
    MarkDirty();
}

void BendiCAMFrame::OnButtonAddToPartClick(wxCommandEvent& event)
{
    if (m_selectedPartIndex < 0 || m_selectedPartIndex >= (int)m_parts.size() || m_selectedIndices.empty())
        return;

    double depthVal = 5.0;
    wxString depthStr = TextCtrlDepth->GetValue();
    depthStr.ToDouble(&depthVal);

    CamPart& part = m_parts[m_selectedPartIndex];

    CamShape shape;
    shape.shapeId = (int)part.shapes.size() + 1;
    shape.depth = depthVal;
    shape.entityIndices = m_selectedIndices;

    part.shapes.push_back(shape);
    part.name = wxString::Format(_("Part %d (%zu shape(s))"), part.id, part.shapes.size()).ToStdString();

    m_selectedIndices.clear();
    RebuildPartsTree();

    UpdatePartButtonStates();

    TextCtrlStatus->AppendText(wxString::Format(_("Added Shape %d (Depth %.1f mm, %zu element(s)) to Part %d\n"),
                                               shape.shapeId, depthVal, shape.entityIndices.size(), part.id));
    PanelDrawing->Refresh();
    Panel3D->Refresh();
    MarkDirty();
}

bool BendiCAMFrame::OpenDxf(const wxString& filename)
{
    if (m_dxfModel.loadFromFile(filename.ToStdString())) {
        m_currentDxfPath = filename.ToStdString();
        m_currentProjectPath.clear();
        m_zoom = 1.0;
        m_panX = 0.0;
        m_panY = 0.0;
        m_isDragging = false;
        m_selectedIndices.clear();
        m_parts.clear();
        m_selectedPartIndex = -1;
        m_selectedShapeInPartIndex = -1;
        m_nextPartId = 1;

        TreeCtrlParts->DeleteAllItems();
        UpdatePartButtonStates();

        CheckListBoxLayers->Clear();
        for (const auto& layer : m_dxfModel.layers) {
            int idx = CheckListBoxLayers->Append(wxString::FromUTF8(layer.name.c_str()));
            CheckListBoxLayers->Check(idx, layer.visible);
        }

        wxFileName fn(filename);
        m_loadedFileName = fn.GetFullName();
        MarkClean();

        TextCtrlStatus->AppendText(wxString::Format(_("Loaded CAD Drawing: %s (%zu entities, %zu layers)\n"),
                                                   filename, m_dxfModel.entities.size(), m_dxfModel.layers.size()));
        PanelDrawing->Refresh();
        Panel3D->Refresh();
        return true;
    } else {
        TextCtrlStatus->AppendText(wxString::Format(_("Failed to load CAD drawing: %s\n"), filename));
        return false;
    }
}

void BendiCAMFrame::OnOpen(wxCommandEvent& event)
{
    wxFileDialog openFileDialog(this, _("Open CAD Drawing"), "", "",
                                _("DXF Files (*.dxf)|*.dxf|All Files (*.*)|*.*"),
                                wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (openFileDialog.ShowModal() == wxID_CANCEL)
        return;

    OpenDxf(openFileDialog.GetPath());
}

bool BendiCAMFrame::OpenProject(const wxString& projectPath)
{
    std::ifstream in(projectPath.ToStdString());
    if (!in.is_open()) {
        TextCtrlStatus->AppendText(wxString::Format(_("Failed to open project file: %s\n"), projectPath));
        return false;
    }

    std::string jsonText((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    JsonParser parser;
    JsonValue root = parser.parse(jsonText);

    std::string cadPath = root["cadFile"].strVal;
    if (cadPath.empty()) {
        TextCtrlStatus->AppendText(_("Invalid project file: missing cadFile reference.\n"));
        return false;
    }

    wxFileName fn(wxString::FromUTF8(cadPath.c_str()));
    if (!fn.FileExists()) {
        wxFileName projFn(projectPath);
        wxString relPath = projFn.GetPath() + wxFileName::GetPathSeparator() + fn.GetFullName();
        if (wxFileName::FileExists(relPath)) {
            cadPath = relPath.ToStdString();
        } else {
            wxFileDialog dxfDlg(this, wxString::Format(_("Locate CAD drawing (%s)"), fn.GetFullName()),
                                projFn.GetPath(), fn.GetFullName(),
                                _("DXF Files (*.dxf)|*.dxf|All Files (*.*)|*.*"),
                                wxFD_OPEN | wxFD_FILE_MUST_EXIST);
            if (dxfDlg.ShowModal() == wxID_CANCEL) return false;
            cadPath = dxfDlg.GetPath().ToStdString();
        }
    }

    if (!m_dxfModel.loadFromFile(cadPath)) {
        TextCtrlStatus->AppendText(wxString::Format(_("Failed to load CAD drawing: %s\n"), cadPath));
        return false;
    }

    m_currentDxfPath = cadPath;
    m_zoom = 1.0;
    m_panX = 0.0;
    m_panY = 0.0;
    m_isDragging = false;
    m_selectedIndices.clear();
    m_parts.clear();
    m_selectedPartIndex = -1;
    m_selectedShapeInPartIndex = -1;
    m_nextPartId = 1;

    // Restore Settings
    bool closeMatching = root["settings"]["closeMatchingEndpoints"].type == JsonValue::BOOL ?
                          root["settings"]["closeMatchingEndpoints"].boolVal :
                          root["settings"]["closeCoincidingPolylines"].boolVal;

    double connTol = root["settings"]["matchingTolerance"].type == JsonValue::NUMBER ?
                     root["settings"]["matchingTolerance"].numVal :
                     root["settings"]["connectionTolerance"].numVal;

    CheckBoxClosePolylines->SetValue(closeMatching);
    TextCtrlConnTolerance->SetValue(wxString::Format(wxT("%.3f"), connTol > 0.0 ? connTol : 0.1));

    // Restore Layers
    const auto& layersArr = root["layers"].arrVal;
    for (const auto& lVal : layersArr) {
        std::string name = lVal["name"].strVal;
        bool visible = lVal["visible"].boolVal;
        DxfLayer* layer = m_dxfModel.findLayer(name);
        if (layer) {
            layer->visible = visible;
        }
    }

    CheckListBoxLayers->Clear();
    for (const auto& layer : m_dxfModel.layers) {
        int idx = CheckListBoxLayers->Append(wxString::FromUTF8(layer.name.c_str()));
        CheckListBoxLayers->Check(idx, layer.visible);
    }

    // Restore Parts
    const auto& partsArr = root["parts"].arrVal;
    int maxId = 0;
    for (const auto& pVal : partsArr) {
        CamPart part;
        part.id = (int)pVal["id"].numVal;
        part.name = pVal["name"].strVal;
        maxId = std::max(maxId, part.id);
        const auto& shapesArr = pVal["shapes"].arrVal;
        for (const auto& sVal : shapesArr) {
            CamShape shape;
            shape.shapeId = (int)sVal["shapeId"].numVal;
            shape.depth = sVal["depth"].numVal;
            const auto& indicesArr = sVal["entityIndices"].arrVal;
            for (const auto& idxVal : indicesArr) {
                shape.entityIndices.insert((size_t)idxVal.numVal);
            }
            part.shapes.push_back(shape);
        }
        m_parts.push_back(part);
    }
    m_nextPartId = maxId + 1;

    // Restore Tool Settings
    if (root["tool"].type == JsonValue::OBJECT) {
        const auto& tObj = root["tool"];
        if (tObj["diameter"].type == JsonValue::NUMBER)
            TextCtrlToolDiameter->SetValue(wxString::Format(wxT("%.3g"), tObj["diameter"].numVal));
        if (tObj["rpm"].type == JsonValue::NUMBER)
            TextCtrlToolRPM->SetValue(wxString::Format(wxT("%.0f"), tObj["rpm"].numVal));
        if (tObj["feedXY"].type == JsonValue::NUMBER)
            TextCtrlToolFeedXY->SetValue(wxString::Format(wxT("%.1f"), tObj["feedXY"].numVal));
        if (tObj["feedZ"].type == JsonValue::NUMBER)
            TextCtrlToolFeedZ->SetValue(wxString::Format(wxT("%.1f"), tObj["feedZ"].numVal));
        if (tObj["stepdown"].type == JsonValue::NUMBER)
            TextCtrlToolStepdown->SetValue(wxString::Format(wxT("%.3g"), tObj["stepdown"].numVal));
        if (tObj["stepover"].type == JsonValue::NUMBER)
            TextCtrlToolStepover->SetValue(wxString::Format(wxT("%.1f"), tObj["stepover"].numVal));
        if (tObj["rampAngle"].type == JsonValue::NUMBER)
            TextCtrlToolRampAngle->SetValue(wxString::Format(wxT("%.1f"), tObj["rampAngle"].numVal));
    }

    // Restore Material Settings
    if (root["material"].type == JsonValue::OBJECT) {
        const auto& mObj = root["material"];
        if (mObj["topHeight"].type == JsonValue::NUMBER)
            TextCtrlMatTopHeight->SetValue(wxString::Format(wxT("%.3g"), mObj["topHeight"].numVal));
        if (mObj["safeZ"].type == JsonValue::NUMBER)
            TextCtrlSafeZ->SetValue(wxString::Format(wxT("%.3g"), mObj["safeZ"].numVal));
    }

    RebuildPartsTree();
    UpdatePartButtonStates();

    wxFileName projFn(projectPath);
    m_loadedFileName = projFn.GetFullName();
    m_currentProjectPath = projectPath.ToStdString();
    MarkClean();

    TextCtrlStatus->AppendText(wxString::Format(_("Loaded BendiCAM Project: %s (%zu part(s))\n"), projectPath, m_parts.size()));
    PanelDrawing->Refresh();
    Panel3D->Refresh();
    return true;
}

void BendiCAMFrame::OnOpenProject(wxCommandEvent& event)
{
    wxFileDialog openFileDialog(this, _("Open BendiCAM Project"), "", "",
                                _("BendiCAM Project (*.bcam)|*.bcam|JSON Files (*.json)|*.json|All Files (*.*)|*.*"),
                                wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (openFileDialog.ShowModal() == wxID_CANCEL)
        return;

    OpenProject(openFileDialog.GetPath());
}

void BendiCAMFrame::OnSaveProject(wxCommandEvent& event)
{
    if (m_dxfModel.entities.empty()) {
        wxMessageBox(_("No CAD drawing loaded. Please open a CAD drawing before saving a project."), _("Save BendiCAM Project"), wxOK | wxICON_INFORMATION, this);
        return;
    }

    wxString defaultName = _T("project.bcam");
    if (!m_currentProjectPath.empty()) {
        wxFileName fn(wxString::FromUTF8(m_currentProjectPath.c_str()));
        defaultName = fn.GetFullName();
    } else if (!m_currentDxfPath.empty()) {
        wxFileName fn(wxString::FromUTF8(m_currentDxfPath.c_str()));
        defaultName = fn.GetName() + _T(".bcam");
    }

    wxFileDialog saveFileDialog(this, _("Save BendiCAM Project"), "", defaultName,
                                _("BendiCAM Project (*.bcam)|*.bcam|JSON Files (*.json)|*.json|All Files (*.*)|*.*"),
                                wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (saveFileDialog.ShowModal() == wxID_CANCEL)
        return;

    wxString filename = saveFileDialog.GetPath();
    if (!filename.EndsWith(_T(".bcam")) && !filename.EndsWith(_T(".json"))) {
        filename += _T(".bcam");
    }

    std::ofstream out(filename.ToStdString());
    if (!out.is_open()) {
        TextCtrlStatus->AppendText(wxString::Format(_("Failed to open file for saving: %s\n"), filename));
        return;
    }

    double connTol = 0.1;
    TextCtrlConnTolerance->GetValue().ToDouble(&connTol);

    double diameter = 2.0, rpm = 12000.0, feedXY = 500.0, feedZ = 250.0, stepdown = 1.0, stepover = 70.0, rampAngle = 20.0;
    double topHeight = 10.0, safeZ = 10.0;

    TextCtrlToolDiameter->GetValue().ToDouble(&diameter);
    TextCtrlToolRPM->GetValue().ToDouble(&rpm);
    TextCtrlToolFeedXY->GetValue().ToDouble(&feedXY);
    TextCtrlToolFeedZ->GetValue().ToDouble(&feedZ);
    TextCtrlToolStepdown->GetValue().ToDouble(&stepdown);
    TextCtrlToolStepover->GetValue().ToDouble(&stepover);
    TextCtrlToolRampAngle->GetValue().ToDouble(&rampAngle);
    TextCtrlMatTopHeight->GetValue().ToDouble(&topHeight);
    TextCtrlSafeZ->GetValue().ToDouble(&safeZ);

    out << "{\n";
    out << "  \"version\": 1,\n";
    out << "  \"cadFile\": \"" << escapeJson(m_currentDxfPath) << "\",\n";
    out << "  \"settings\": {\n";
    out << "    \"closeMatchingEndpoints\": " << (CheckBoxClosePolylines->IsChecked() ? "true" : "false") << ",\n";
    out << "    \"matchingTolerance\": " << connTol << "\n";
    out << "  },\n";
    out << "  \"tool\": {\n";
    out << "    \"diameter\": " << diameter << ",\n";
    out << "    \"rpm\": " << rpm << ",\n";
    out << "    \"feedXY\": " << feedXY << ",\n";
    out << "    \"feedZ\": " << feedZ << ",\n";
    out << "    \"stepdown\": " << stepdown << ",\n";
    out << "    \"stepover\": " << stepover << ",\n";
    out << "    \"rampAngle\": " << rampAngle << "\n";
    out << "  },\n";
    out << "  \"material\": {\n";
    out << "    \"topHeight\": " << topHeight << ",\n";
    out << "    \"safeZ\": " << safeZ << "\n";
    out << "  },\n";

    out << "  \"layers\": [\n";
    for (size_t i = 0; i < m_dxfModel.layers.size(); ++i) {
        const auto& layer = m_dxfModel.layers[i];
        out << "    {\n";
        out << "      \"name\": \"" << escapeJson(layer.name) << "\",\n";
        out << "      \"visible\": " << (layer.visible ? "true" : "false") << "\n";
        out << "    }" << (i + 1 < m_dxfModel.layers.size() ? "," : "") << "\n";
    }
    out << "  ],\n";

    out << "  \"parts\": [\n";
    for (size_t pIdx = 0; pIdx < m_parts.size(); ++pIdx) {
        const auto& part = m_parts[pIdx];
        out << "    {\n";
        out << "      \"id\": " << part.id << ",\n";
        out << "      \"name\": \"" << escapeJson(part.name) << "\",\n";
        out << "      \"shapes\": [\n";
        for (size_t sIdx = 0; sIdx < part.shapes.size(); ++sIdx) {
            const auto& shape = part.shapes[sIdx];
            out << "        {\n";
            out << "          \"shapeId\": " << shape.shapeId << ",\n";
            out << "          \"depth\": " << shape.depth << ",\n";
            out << "          \"entityIndices\": [";
            size_t count = 0;
            for (size_t idx : shape.entityIndices) {
                out << idx << (count + 1 < shape.entityIndices.size() ? ", " : "");
                count++;
            }
            out << "]\n";
            out << "        }" << (sIdx + 1 < part.shapes.size() ? "," : "") << "\n";
        }
        out << "      ]\n";
        out << "    }" << (pIdx + 1 < m_parts.size() ? "," : "") << "\n";
    }
    out << "  ]\n";
    out << "}\n";

    out.close();
    wxFileName saveFn(filename);
    m_loadedFileName = saveFn.GetFullName();
    m_currentProjectPath = filename.ToStdString();
    MarkClean();
    TextCtrlStatus->AppendText(wxString::Format(_("Saved BendiCAM Project: %s\n"), filename));
}

void BendiCAMFrame::OnCheckListBoxLayersToggled(wxCommandEvent& event)
{
    unsigned int n = CheckListBoxLayers->GetCount();
    for (unsigned int i = 0; i < n; ++i) {
        wxString name = CheckListBoxLayers->GetString(i);
        bool checked = CheckListBoxLayers->IsChecked(i);
        DxfLayer* layer = m_dxfModel.findLayer(name.ToStdString());
        if (layer) {
            layer->visible = checked;
        }
    }
    m_selectedIndices.clear();
    m_dxfModel.calculateBoundingBox();
    PanelDrawing->Refresh();
}

void BendiCAMFrame::OnPanelEraseBg(wxEraseEvent& event)
{
}

void BendiCAMFrame::OnPanel3DEraseBg(wxEraseEvent& event)
{
}

void BendiCAMFrame::OnPanel3DLeftDown(wxMouseEvent& event)
{
    m_isDragging3DRot = true;
    m_lastMousePos3D = event.GetPosition();
    if (!Panel3D->HasCapture()) {
        Panel3D->CaptureMouse();
    }
}

void BendiCAMFrame::OnPanel3DLeftUp(wxMouseEvent& event)
{
    if (m_isDragging3DRot) {
        m_isDragging3DRot = false;
        if (Panel3D->HasCapture()) {
            Panel3D->ReleaseMouse();
        }
    }
}

void BendiCAMFrame::OnPanel3DMiddleDown(wxMouseEvent& event)
{
    m_isDragging3DPan = true;
    m_lastMousePos3D = event.GetPosition();
    if (!Panel3D->HasCapture()) {
        Panel3D->CaptureMouse();
    }
}

void BendiCAMFrame::OnPanel3DMiddleUp(wxMouseEvent& event)
{
    if (m_isDragging3DPan) {
        m_isDragging3DPan = false;
        if (Panel3D->HasCapture()) {
            Panel3D->ReleaseMouse();
        }
    }
}

void BendiCAMFrame::OnPanel3DMouseMove(wxMouseEvent& event)
{
    if (m_isDragging3DRot && event.LeftIsDown()) {
        wxPoint currentPos = event.GetPosition();
        int dx = currentPos.x - m_lastMousePos3D.x;
        int dy = currentPos.y - m_lastMousePos3D.y;

        m_rotY3D += dx * 0.5;
        m_rotX3D += dy * 0.5;

        m_lastMousePos3D = currentPos;
        Panel3D->Refresh();
    } else if (m_isDragging3DPan && event.MiddleIsDown()) {
        wxPoint currentPos = event.GetPosition();
        int dx = currentPos.x - m_lastMousePos3D.x;
        int dy = currentPos.y - m_lastMousePos3D.y;

        m_panX3D += dx;
        m_panY3D += dy;

        m_lastMousePos3D = currentPos;
        Panel3D->Refresh();
    } else {
        if (m_isDragging3DRot && !event.LeftIsDown()) {
            m_isDragging3DRot = false;
            if (Panel3D->HasCapture()) Panel3D->ReleaseMouse();
        }
        if (m_isDragging3DPan && !event.MiddleIsDown()) {
            m_isDragging3DPan = false;
            if (Panel3D->HasCapture()) Panel3D->ReleaseMouse();
        }
    }
}

void BendiCAMFrame::OnPanel3DMouseWheel(wxMouseEvent& event)
{
    int rotation = event.GetWheelRotation();
    if (rotation == 0) return;

    double zoomFactor = (rotation > 0) ? 1.15 : (1.0 / 1.15);
    m_zoom3D *= zoomFactor;
    if (m_zoom3D < 0.05) m_zoom3D = 0.05;
    if (m_zoom3D > 200.0) m_zoom3D = 200.0;

    Panel3D->Refresh();
}

void BendiCAMFrame::OnPanelMiddleDown(wxMouseEvent& event)
{
    m_isDragging = true;
    m_lastMousePos = event.GetPosition();
    if (!PanelDrawing->HasCapture()) {
        PanelDrawing->CaptureMouse();
    }
}

void BendiCAMFrame::OnPanelMiddleUp(wxMouseEvent& event)
{
    if (m_isDragging) {
        m_isDragging = false;
        if (PanelDrawing->HasCapture()) {
            PanelDrawing->ReleaseMouse();
        }
    }
}

void BendiCAMFrame::OnPanelMouseMove(wxMouseEvent& event)
{
    if (m_isDragging && event.MiddleIsDown()) {
        wxPoint currentPos = event.GetPosition();
        int dx = currentPos.x - m_lastMousePos.x;
        int dy = currentPos.y - m_lastMousePos.y;

        m_panX += dx;
        m_panY += dy;

        m_lastMousePos = currentPos;
        PanelDrawing->Refresh();
    } else if (m_isDragging && !event.MiddleIsDown()) {
        m_isDragging = false;
        if (PanelDrawing->HasCapture()) {
            PanelDrawing->ReleaseMouse();
        }
    }
}

void BendiCAMFrame::OnPanelMouseWheel(wxMouseEvent& event)
{
    if (!m_dxfModel.bbox.valid || m_dxfModel.entities.empty())
        return;

    int rotation = event.GetWheelRotation();
    if (rotation == 0) return;

    wxSize size = PanelDrawing->GetClientSize();
    int pw = size.GetWidth();
    int ph = size.GetHeight();
    if (pw <= 0 || ph <= 0) return;

    double margin = 20.0;
    double availW = pw - 2.0 * margin;
    double availH = ph - 2.0 * margin;
    if (availW <= 0 || availH <= 0) return;

    double bboxW = m_dxfModel.bbox.maxX - m_dxfModel.bbox.minX;
    double bboxH = m_dxfModel.bbox.maxY - m_dxfModel.bbox.minY;
    if (bboxW <= 0.000001) bboxW = 1.0;
    if (bboxH <= 0.000001) bboxH = 1.0;

    double baseScale = std::min(availW / bboxW, availH / bboxH);
    double oldScale = baseScale * m_zoom;

    double zoomFactor = (rotation > 0) ? 1.15 : (1.0 / 1.15);
    double newZoom = m_zoom * zoomFactor;

    if (newZoom < 0.05) newZoom = 0.05;
    if (newZoom > 200.0) newZoom = 200.0;

    double newScale = baseScale * newZoom;

    wxPoint mousePos = event.GetPosition();
    double mx = mousePos.x;
    double my = mousePos.y;

    double baseOffsetX_old = margin + (availW - bboxW * oldScale) / 2.0;
    double baseOffsetY_old = margin + (availH - bboxH * oldScale) / 2.0;
    double totalOffsetX_old = baseOffsetX_old + m_panX;
    double totalOffsetY_old = baseOffsetY_old + m_panY;

    double dxfX = m_dxfModel.bbox.minX + (mx - totalOffsetX_old) / oldScale;
    double dxfY = m_dxfModel.bbox.maxY - (my - totalOffsetY_old) / oldScale;

    m_zoom = newZoom;

    double baseOffsetX_new = margin + (availW - bboxW * newScale) / 2.0;
    double baseOffsetY_new = margin + (availH - bboxH * newScale) / 2.0;

    m_panX = mx - baseOffsetX_new - (dxfX - m_dxfModel.bbox.minX) * newScale;
    m_panY = my - baseOffsetY_new - (m_dxfModel.bbox.maxY - dxfY) * newScale;

    PanelDrawing->Refresh();
}

void BendiCAMFrame::OnPanelLeftDown(wxMouseEvent& event)
{
    if (!m_dxfModel.bbox.valid || m_dxfModel.entities.empty())
        return;

    wxSize size = PanelDrawing->GetClientSize();
    int pw = size.GetWidth();
    int ph = size.GetHeight();
    if (pw <= 0 || ph <= 0) return;

    double margin = 20.0;
    double availW = pw - 2.0 * margin;
    double availH = ph - 2.0 * margin;
    if (availW <= 0 || availH <= 0) return;

    double bboxW = m_dxfModel.bbox.maxX - m_dxfModel.bbox.minX;
    double bboxH = m_dxfModel.bbox.maxY - m_dxfModel.bbox.minY;
    if (bboxW <= 0.000001) bboxW = 1.0;
    if (bboxH <= 0.000001) bboxH = 1.0;

    double baseScale = std::min(availW / bboxW, availH / bboxH);
    double scale = baseScale * m_zoom;

    double drawnW = bboxW * scale;
    double drawnH = bboxH * scale;

    double offsetX = margin + (availW - drawnW) / 2.0 + m_panX;
    double offsetY = margin + (availH - drawnH) / 2.0 + m_panY;

    wxPoint mousePos = event.GetPosition();
    double clickX = m_dxfModel.bbox.minX + (mousePos.x - offsetX) / scale;
    double clickY = m_dxfModel.bbox.maxY - (mousePos.y - offsetY) / scale;

    double tolerancePx = 10.0;
    double toleranceDxf = tolerancePx / scale;

    int clickedEntityIdx = m_dxfModel.hitTest(clickX, clickY, toleranceDxf);
    if (clickedEntityIdx >= 0) {
        bool closeCoinciding = CheckBoxClosePolylines->IsChecked();
        double connTol = 0.1;
        wxString tolStr = TextCtrlConnTolerance->GetValue();
        if (!tolStr.ToDouble(&connTol) || connTol <= 0.0) {
            connTol = 2.0 / scale;
        }
        m_selectedIndices = m_dxfModel.findConnectedEntities((size_t)clickedEntityIdx, connTol, closeCoinciding);
        TextCtrlStatus->AppendText(wxString::Format(_("Selected shape (%zu connected element(s))\n"), m_selectedIndices.size()));
    } else {
        m_selectedIndices.clear();
        m_selectedPartIndex = -1;
        m_selectedShapeInPartIndex = -1;
        TreeCtrlParts->UnselectAll();
        TextCtrlStatus->AppendText(_("Selection cleared\n"));
    }

    UpdatePartButtonStates();
    PanelDrawing->Refresh();
}

std::vector<AnalyticalPath> BendiCAMFrame::getShapeAnalyticalPaths(const CamShape& shape) const
{
    std::vector<size_t> indices(shape.entityIndices.begin(), shape.entityIndices.end());
    return ::getShapeAnalyticalPaths(m_dxfModel, indices);
}

std::vector<std::vector<DxfPoint>> BendiCAMFrame::getShapeClosedPaths(const CamShape& shape) const
{
    auto analyticalPaths = getShapeAnalyticalPaths(shape);
    std::vector<std::vector<DxfPoint>> closedResult;
    for (const auto& path : analyticalPaths) {
        auto pts = sampleAnalyticalPath(path, 0.5);
        if (!pts.empty()) {
            closedResult.push_back(pts);
        }
    }
    return closedResult;
}

void BendiCAMFrame::OnPanelPaint(wxPaintEvent& event)
{
    wxAutoBufferedPaintDC dc(PanelDrawing);
    dc.SetBackground(*wxWHITE_BRUSH);
    dc.Clear();

    if (!m_dxfModel.bbox.valid || m_dxfModel.entities.empty()) {
        return;
    }

    wxSize size = PanelDrawing->GetClientSize();
    int pw = size.GetWidth();
    int ph = size.GetHeight();
    if (pw <= 0 || ph <= 0) return;

    double margin = 20.0;
    double availW = pw - 2.0 * margin;
    double availH = ph - 2.0 * margin;
    if (availW <= 0 || availH <= 0) return;

    double bboxW = m_dxfModel.bbox.maxX - m_dxfModel.bbox.minX;
    double bboxH = m_dxfModel.bbox.maxY - m_dxfModel.bbox.minY;

    if (bboxW <= 0.000001) bboxW = 1.0;
    if (bboxH <= 0.000001) bboxH = 1.0;

    double baseScale = std::min(availW / bboxW, availH / bboxH);
    double scale = baseScale * m_zoom;

    double drawnW = bboxW * scale;
    double drawnH = bboxH * scale;

    double offsetX = margin + (availW - drawnW) / 2.0 + m_panX;
    double offsetY = margin + (availH - drawnH) / 2.0 + m_panY;

    auto ToScreen = [&](double x, double y) -> wxPoint {
        int px = (int)(offsetX + (x - m_dxfModel.bbox.minX) * scale);
        int py = (int)(offsetY + (m_dxfModel.bbox.maxY - y) * scale);
        return wxPoint(px, py);
    };

    auto drawEntityGeometry = [&](const DxfEntity& entity) {
        if (entity.type == DxfEntityType::LINE && entity.points.size() >= 2) {
            wxPoint p1 = ToScreen(entity.points[0].x, entity.points[0].y);
            wxPoint p2 = ToScreen(entity.points[1].x, entity.points[1].y);
            dc.DrawLine(p1, p2);
        }
        else if (entity.type == DxfEntityType::POLYLINE) {
            if (!entity.polylineVertices.empty()) {
                size_t count = entity.polylineVertices.size();
                size_t numSegs = entity.isClosed ? count : (count - 1);
                for (size_t i = 0; i < numSegs; ++i) {
                    const auto& v1 = entity.polylineVertices[i];
                    const auto& v2 = entity.polylineVertices[(i + 1) % count];
                    double b = v1.bulge;
                    if (std::abs(b) > 1e-6) {
                        double dx = v2.x - v1.x;
                        double dy = v2.y - v1.y;
                        double d = std::hypot(dx, dy);
                        if (d > 1e-6) {
                            double s = (d * (1.0 - b * b)) / (4.0 * b);
                            double mx = (v1.x + v2.x) / 2.0;
                            double my = (v1.y + v2.y) / 2.0;
                            double cx = mx - s * (dy / d);
                            double cy = my + s * (dx / d);

                            wxPoint pStart = ToScreen(v1.x, v1.y);
                            wxPoint pEnd = ToScreen(v2.x, v2.y);
                            wxPoint pCenter = ToScreen(cx, cy);

                            if (pStart != pEnd) {
                                dc.SetBrush(*wxTRANSPARENT_BRUSH);
                                if (b > 0) {
                                    dc.DrawArc(pStart, pEnd, pCenter);
                                } else {
                                    dc.DrawArc(pEnd, pStart, pCenter);
                                }
                            }
                        }
                    } else {
                        wxPoint p1 = ToScreen(v1.x, v1.y);
                        wxPoint p2 = ToScreen(v2.x, v2.y);
                        dc.DrawLine(p1, p2);
                    }
                }
            } else if (entity.points.size() >= 2) {
                std::vector<wxPoint> pts;
                pts.reserve(entity.points.size());
                for (const auto& pt : entity.points) {
                    pts.push_back(ToScreen(pt.x, pt.y));
                }
                dc.DrawLines((int)pts.size(), pts.data());
                if (entity.isClosed && pts.size() > 2) {
                    dc.DrawLine(pts.back(), pts.front());
                }
            }
        }
        else if (entity.type == DxfEntityType::SPLINE && entity.points.size() >= 2) {
            std::vector<wxPoint> pts;
            pts.reserve(entity.points.size());
            for (const auto& pt : entity.points) {
                pts.push_back(ToScreen(pt.x, pt.y));
            }
            dc.DrawSpline((int)pts.size(), pts.data());
        }
        else if (entity.type == DxfEntityType::CIRCLE) {
            wxPoint center = ToScreen(entity.center.x, entity.center.y);
            int r = (int)(entity.radius * scale);
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.DrawCircle(center, std::max(1, r));
        }
        else if (entity.type == DxfEntityType::ARC) {
            double startRad = entity.startAngle * M_PI / 180.0;
            double endRad = entity.endAngle * M_PI / 180.0;

            double sx = entity.center.x + entity.radius * std::cos(startRad);
            double sy = entity.center.y + entity.radius * std::sin(startRad);
            double ex = entity.center.x + entity.radius * std::cos(endRad);
            double ey = entity.center.y + entity.radius * std::sin(endRad);

            wxPoint pStart = ToScreen(sx, sy);
            wxPoint pEnd = ToScreen(ex, ey);
            wxPoint pCenter = ToScreen(entity.center.x, entity.center.y);

            if (pStart == pEnd) {
                int r = (int)(entity.radius * scale);
                dc.SetBrush(*wxTRANSPARENT_BRUSH);
                dc.DrawCircle(pCenter, std::max(1, r));
            } else {
                dc.SetBrush(*wxTRANSPARENT_BRUSH);
                dc.DrawArc(pStart, pEnd, pCenter);
            }
        }
        else if (entity.type == DxfEntityType::ELLIPSE) {
            wxPoint centerSc = ToScreen(entity.center.x, entity.center.y);
            double aLen = std::hypot(entity.majorAxis.x, entity.majorAxis.y) * scale;
            double bLen = aLen * entity.ratio;
            double angle = std::atan2(entity.majorAxis.y, entity.majorAxis.x);

            std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
            if (gc && aLen > 0.5) {
                gc->SetPen(dc.GetPen());
                gc->SetBrush(*wxTRANSPARENT_BRUSH);
                gc->PushState();
                gc->Translate(centerSc.x, centerSc.y);
                gc->Rotate(-angle);
                wxGraphicsPath gpath = gc->CreatePath();
                bool isFull = (std::abs(entity.endParam - entity.startParam - 2.0 * M_PI) < 1e-3) ||
                              (entity.startParam == 0.0 && entity.endParam >= 2.0 * M_PI - 1e-3);
                if (isFull) {
                    gpath.AddEllipse(-aLen, -bLen, 2.0 * aLen, 2.0 * bLen);
                } else {
                    gpath.AddArc(0, 0, aLen, -entity.startParam, -entity.endParam, false);
                }
                gc->StrokePath(gpath);
                gc->PopState();
            } else {
                int r = (int)(aLen);
                dc.SetBrush(*wxTRANSPARENT_BRUSH);
                dc.DrawCircle(centerSc, std::max(1, r));
            }
        }
        else if (entity.type == DxfEntityType::SOLID && entity.points.size() >= 4) {
            std::vector<wxPoint> pts;
            pts.push_back(ToScreen(entity.points[0].x, entity.points[0].y));
            pts.push_back(ToScreen(entity.points[1].x, entity.points[1].y));
            pts.push_back(ToScreen(entity.points[3].x, entity.points[3].y));
            pts.push_back(ToScreen(entity.points[2].x, entity.points[2].y));
            dc.DrawLines((int)pts.size(), pts.data());
            dc.DrawLine(pts.back(), pts.front());
        }
        else if (entity.type == DxfEntityType::TEXT && !entity.points.empty() && !entity.textString.empty()) {
            wxPoint pt = ToScreen(entity.points[0].x, entity.points[0].y);
            dc.DrawText(wxString::FromUTF8(entity.textString.c_str()), pt);
        }
        else if (entity.type == DxfEntityType::POINT && !entity.points.empty()) {
            wxPoint pt = ToScreen(entity.points[0].x, entity.points[0].y);
            dc.DrawPoint(pt);
        }
    };

    double minDepth = 0.0, maxDepth = 0.0;
    getMinMaxDepth(minDepth, maxDepth);

    // Pass -1: Filled Shapes for All Existing Parts (Lowest depth to highest depth, showing topmost color)
    struct ShapeFillItem {
        double depth;
        wxColour fillCol;
        std::vector<std::vector<wxPoint>> screenPaths;
    };

    std::vector<ShapeFillItem> fillItems;
    for (const auto& part : m_parts) {
        for (const auto& shape : part.shapes) {
            ShapeFillItem item;
            item.depth = shape.depth;
            wxColour depthCol = getDepthColor(shape.depth, minDepth, maxDepth);
            item.fillCol = wxColour(depthCol.Red(), depthCol.Green(), depthCol.Blue(), 160);

            auto paths = getShapeClosedPaths(shape);
            for (const auto& pathPts : paths) {
                if (pathPts.size() >= 3) {
                    std::vector<wxPoint> pts;
                    pts.reserve(pathPts.size());
                    for (const auto& pt : pathPts) {
                        pts.push_back(ToScreen(pt.x, pt.y));
                    }
                    item.screenPaths.push_back(pts);
                }
            }
            if (!item.screenPaths.empty()) {
                fillItems.push_back(item);
            }
        }
    }

    // Sort lowest depth to highest depth so higher (topmost) shapes paint over lower ones
    std::sort(fillItems.begin(), fillItems.end(), [](const ShapeFillItem& a, const ShapeFillItem& b) {
        return a.depth < b.depth;
    });

    // Pass -1 & Pass -0.5: Scoped wxGraphicsContext block (flushes Direct2D rendering before wxAutoBufferedPaintDC blits)
    {
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (gc) {
            for (const auto& item : fillItems) {
                gc->SetBrush(gc->CreateBrush(wxBrush(item.fillCol)));
                gc->SetPen(*wxTRANSPARENT_PEN);

                for (const auto& pts : item.screenPaths) {
                    if (pts.size() >= 3) {
                        wxGraphicsPath gpath = gc->CreatePath();
                        gpath.MoveToPoint(pts[0].x, pts[0].y);
                        for (size_t i = 1; i < pts.size(); ++i) {
                            gpath.AddLineToPoint(pts[i].x, pts[i].y);
                        }
                        gpath.CloseSubpath();
                        gc->FillPath(gpath);
                    }
                }
            }

            // Pass -0.5: Milling Area for Selected Shape / Part in Parts List
            if (m_selectedPartIndex >= 0 && m_selectedPartIndex < (int)m_parts.size()) {
                const auto& part = m_parts[m_selectedPartIndex];

                // Get Tool Diameter and Stepover
                double toolDiameter = 2.0;
                double stepoverPercent = 70.0;
                if (TextCtrlToolDiameter) TextCtrlToolDiameter->GetValue().ToDouble(&toolDiameter);
                if (TextCtrlToolStepover) TextCtrlToolStepover->GetValue().ToDouble(&stepoverPercent);
                if (toolDiameter <= 1e-4) toolDiameter = 2.0;
                if (stepoverPercent < 0.0) stepoverPercent = 70.0;

                double extensionFraction = std::max(0.0, (100.0 - stepoverPercent) / 100.0);
                double extensionDist = toolDiameter * extensionFraction;

                std::vector<int> targetShapeIndices;
                if (m_selectedShapeInPartIndex >= 0 && m_selectedShapeInPartIndex < (int)part.shapes.size()) {
                    targetShapeIndices.push_back(m_selectedShapeInPartIndex);
                } else {
                    for (int i = 0; i < (int)part.shapes.size(); ++i) {
                        targetShapeIndices.push_back(i);
                    }
                }

                for (int shapeIdx : targetShapeIndices) {
                    const auto& selShape = part.shapes[shapeIdx];
                    double selDepth = selShape.depth;

                    auto selAnalyticalPaths = getShapeAnalyticalPaths(selShape);
                    if (selAnalyticalPaths.empty()) continue;

                    std::vector<DxfPoint> selSamplePts;
                    for (const auto& sp : selAnalyticalPaths) {
                        auto pts = sampleAnalyticalPath(sp, 0.5);
                        selSamplePts.insert(selSamplePts.end(), pts.begin(), pts.end());
                    }

                    // Identify "shape below" in the part
                    const CamShape* shapeBelow = nullptr;
                    double maxBelowDepth = -1.0;

                    for (size_t sIdx = 0; sIdx < part.shapes.size(); ++sIdx) {
                        if ((int)sIdx == shapeIdx) continue;
                        const auto& candidateShape = part.shapes[sIdx];
                        if (candidateShape.depth < selDepth - 1e-4) {
                            auto candPaths = getShapeAnalyticalPaths(candidateShape);
                            bool surrounds = false;
                            if (!selSamplePts.empty() && !candPaths.empty()) {
                                int insideCount = 0;
                                for (const auto& pt : selSamplePts) {
                                    for (const auto& cPath : candPaths) {
                                        if (isPointInsideAnalyticalPath(pt, cPath)) {
                                            insideCount++;
                                            break;
                                        }
                                    }
                                }
                                if ((double)insideCount / selSamplePts.size() > 0.5) {
                                    surrounds = true;
                                }
                            }

                            if (surrounds) {
                                if (shapeBelow == nullptr || candidateShape.depth > maxBelowDepth) {
                                    shapeBelow = &candidateShape;
                                    maxBelowDepth = candidateShape.depth;
                                }
                            }
                        }
                    }

                    if (!shapeBelow) {
                        for (size_t sIdx = 0; sIdx < part.shapes.size(); ++sIdx) {
                            if ((int)sIdx == shapeIdx) continue;
                            const auto& candidateShape = part.shapes[sIdx];
                            if (candidateShape.depth < selDepth - 1e-4) {
                                if (shapeBelow == nullptr || candidateShape.depth > maxBelowDepth) {
                                    shapeBelow = &candidateShape;
                                    maxBelowDepth = candidateShape.depth;
                                }
                            }
                        }
                    }

                    struct LoopScreenPoints {
                        std::vector<wxPoint> outerPts;
                        std::vector<wxPoint> innerPts;
                    };
                    std::vector<LoopScreenPoints> millingLoops;

                    if (shapeBelow != nullptr) {
                        auto belowPaths = getShapeAnalyticalPaths(*shapeBelow);

                        for (const auto& selPath : selAnalyticalPaths) {
                            AnalyticalPath selLoop = selPath;
                            ensureLoopOrientation(selLoop, true);
                            auto selPts = sampleAnalyticalPath(selLoop, 0.5);

                            for (const auto& bPath : belowPaths) {
                                AnalyticalPath bLoop = bPath;
                                ensureLoopOrientation(bLoop, true);
                                auto expandedOuter = offsetAnalyticalPath(bLoop, -extensionDist);

                                for (const auto& outerLoop : expandedOuter) {
                                    auto outerPts = sampleAnalyticalPath(outerLoop, 0.5);

                                    LoopScreenPoints lsp;
                                    for (const auto& pt : outerPts) {
                                        lsp.outerPts.push_back(ToScreen(pt.x, pt.y));
                                    }
                                    for (const auto& pt : selPts) {
                                        lsp.innerPts.push_back(ToScreen(pt.x, pt.y));
                                    }
                                    if (lsp.outerPts.size() >= 3 && lsp.innerPts.size() >= 3) {
                                        millingLoops.push_back(lsp);
                                    }
                                }
                            }
                        }
                    } else {
                        for (const auto& selPath : selAnalyticalPaths) {
                            AnalyticalPath selLoop = selPath;
                            ensureLoopOrientation(selLoop, true);
                            auto selPts = sampleAnalyticalPath(selLoop, 0.5);

                            auto outerCutout = offsetAnalyticalPath(selLoop, -toolDiameter);
                            for (const auto& outerLoop : outerCutout) {
                                auto outerPts = sampleAnalyticalPath(outerLoop, 0.5);

                                LoopScreenPoints lsp;
                                for (const auto& pt : outerPts) {
                                    lsp.outerPts.push_back(ToScreen(pt.x, pt.y));
                                }
                                for (const auto& pt : selPts) {
                                    lsp.innerPts.push_back(ToScreen(pt.x, pt.y));
                                }
                                if (lsp.outerPts.size() >= 3 && lsp.innerPts.size() >= 3) {
                                    millingLoops.push_back(lsp);
                                }
                            }
                        }
                    }

                    if (!millingLoops.empty()) {
                        wxColour fillCol(255, 170, 0, 85);        // Translucent orange/amber milling area
                        wxColour penCol(230, 100, 0);              // Darker amber for dashed borders
                        wxColour cutoutWhite(255, 255, 255, 255); // Solid white cutout fill

                        for (const auto& lsp : millingLoops) {
                            // 1. Fill milling area (ring between outer boundary and inner shape)
                            wxGraphicsPath gpath = gc->CreatePath();
                            gpath.MoveToPoint(lsp.outerPts[0].x, lsp.outerPts[0].y);
                            for (size_t i = 1; i < lsp.outerPts.size(); ++i) {
                                gpath.AddLineToPoint(lsp.outerPts[i].x, lsp.outerPts[i].y);
                            }
                            gpath.CloseSubpath();

                            if (!lsp.innerPts.empty()) {
                                gpath.MoveToPoint(lsp.innerPts.back().x, lsp.innerPts.back().y);
                                for (int i = (int)lsp.innerPts.size() - 2; i >= 0; --i) {
                                    gpath.AddLineToPoint(lsp.innerPts[i].x, lsp.innerPts[i].y);
                                }
                                gpath.CloseSubpath();
                            }

                            gc->SetBrush(gc->CreateBrush(wxBrush(fillCol)));
                            gc->SetPen(*wxTRANSPARENT_PEN);
                            gc->FillPath(gpath, wxWINDING_RULE);

                            // 2. Cut out selected shape interior (white/hollow background fill)
                            if (lsp.innerPts.size() >= 3) {
                                wxGraphicsPath innerShapePath = gc->CreatePath();
                                innerShapePath.MoveToPoint(lsp.innerPts[0].x, lsp.innerPts[0].y);
                                for (size_t i = 1; i < lsp.innerPts.size(); ++i) {
                                    innerShapePath.AddLineToPoint(lsp.innerPts[i].x, lsp.innerPts[i].y);
                                }
                                innerShapePath.CloseSubpath();

                                gc->SetBrush(gc->CreateBrush(wxBrush(cutoutWhite)));
                                gc->SetPen(*wxTRANSPARENT_PEN);
                                gc->FillPath(innerShapePath);
                            }

                            // 3. Apply dashed darker stroke to both outer milling boundary and inner cutout border
                            gc->SetPen(wxPen(penCol, 2, wxPENSTYLE_SHORT_DASH));
                            gc->SetBrush(*wxTRANSPARENT_BRUSH);

                            if (lsp.outerPts.size() >= 3) {
                                wxGraphicsPath outerBorderPath = gc->CreatePath();
                                outerBorderPath.MoveToPoint(lsp.outerPts[0].x, lsp.outerPts[0].y);
                                for (size_t i = 1; i < lsp.outerPts.size(); ++i) {
                                    outerBorderPath.AddLineToPoint(lsp.outerPts[i].x, lsp.outerPts[i].y);
                                }
                                outerBorderPath.CloseSubpath();
                                gc->StrokePath(outerBorderPath);
                            }

                            if (lsp.innerPts.size() >= 3) {
                                wxGraphicsPath innerBorderPath = gc->CreatePath();
                                innerBorderPath.MoveToPoint(lsp.innerPts[0].x, lsp.innerPts[0].y);
                                for (size_t i = 1; i < lsp.innerPts.size(); ++i) {
                                    innerBorderPath.AddLineToPoint(lsp.innerPts[i].x, lsp.innerPts[i].y);
                                }
                                innerBorderPath.CloseSubpath();
                                gc->StrokePath(innerBorderPath);
                            }
                        }
                    }
                }
            }
        }
    }

    // Pass 0: Selected Part / Shape Highlighter Pass (glowing depth-gradient shadow pen underneath)
    if (m_selectedPartIndex >= 0 && m_selectedPartIndex < (int)m_parts.size()) {
        const auto& part = m_parts[m_selectedPartIndex];
        dc.SetBrush(*wxTRANSPARENT_BRUSH);

        for (int sIdx = 0; sIdx < (int)part.shapes.size(); ++sIdx) {
            const auto& shape = part.shapes[sIdx];
            wxColour glowCol = getDepthColor(shape.depth, minDepth, maxDepth);

            int penWidth = 5;
            if (m_selectedShapeInPartIndex >= 0) {
                if (sIdx == m_selectedShapeInPartIndex) penWidth = 7;
                else penWidth = 4;
            }

            dc.SetPen(wxPen(glowCol, penWidth, wxPENSTYLE_SOLID));
            for (size_t idx : shape.entityIndices) {
                if (idx < m_dxfModel.entities.size()) {
                    const auto& entity = m_dxfModel.entities[idx];
                    const DxfLayer* layer = m_dxfModel.findLayer(entity.layerName);
                    if (!layer || layer->visible) {
                        drawEntityGeometry(entity);
                    }
                }
            }
        }
    }

    // Pass 1: Selection Highlighter Pass (glowing amber/gold shadow pen underneath)
    if (!m_selectedIndices.empty()) {
        wxColour highlightCol(255, 200, 0); // Gold / Amber glow
        dc.SetPen(wxPen(highlightCol, 5, wxPENSTYLE_SOLID));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        for (size_t idx : m_selectedIndices) {
            if (idx < m_dxfModel.entities.size()) {
                const auto& entity = m_dxfModel.entities[idx];
                const DxfLayer* layer = m_dxfModel.findLayer(entity.layerName);
                if (!layer || layer->visible) {
                    drawEntityGeometry(entity);
                }
            }
        }
    }

    // Pass 2: Normal Rendering Pass
    for (const auto& entity : m_dxfModel.entities) {
        const DxfLayer* layer = m_dxfModel.findLayer(entity.layerName);
        if (layer && !layer->visible) {
            continue;
        }

        DxfColor color = m_dxfModel.getEntityColor(entity);
        wxColour wxCol(color.r, color.g, color.b);
        dc.SetPen(wxPen(wxCol, 1, wxPENSTYLE_SOLID));
        dc.SetTextForeground(wxCol);

        drawEntityGeometry(entity);
    }
}

void BendiCAMFrame::OnPanel3DPaint(wxPaintEvent& event)
{
    wxAutoBufferedPaintDC dc(Panel3D);
    dc.SetBackground(wxBrush(wxColour(245, 245, 250)));
    dc.Clear();

    wxSize size = Panel3D->GetClientSize();
    int pw = size.GetWidth();
    int ph = size.GetHeight();
    if (pw <= 0 || ph <= 0) return;

    dc.SetFont(wxFont(9, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    dc.SetTextForeground(wxColour(80, 80, 90));
    dc.DrawText(_("3D Part View"), 10, 8);

    if (m_selectedPartIndex < 0 || m_selectedPartIndex >= (int)m_parts.size()) {
        dc.SetFont(wxFont(9, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_ITALIC, wxFONTWEIGHT_NORMAL));
        dc.SetTextForeground(wxColour(140, 140, 150));
        dc.DrawText(_("Select a Part to view 3D extrusion"), 10, 30);
        return;
    }

    const auto& part = m_parts[m_selectedPartIndex];
    if (part.shapes.empty()) return;

    DxfBoundingBox bbox3D;
    double maxPartDepth = 0.0;

    for (const auto& shape : part.shapes) {
        maxPartDepth = std::max(maxPartDepth, shape.depth);
        for (size_t idx : shape.entityIndices) {
            if (idx < m_dxfModel.entities.size()) {
                const auto& ent = m_dxfModel.entities[idx];
                for (const auto& pt : ent.points) {
                    bbox3D.update(pt.x, pt.y);
                }
                if (ent.type == DxfEntityType::CIRCLE || ent.type == DxfEntityType::ARC) {
                    bbox3D.update(ent.center.x - ent.radius, ent.center.y - ent.radius);
                    bbox3D.update(ent.center.x + ent.radius, ent.center.y + ent.radius);
                }
            }
        }
    }

    if (!bbox3D.valid) return;

    double minDepth = 0.0, maxDepthAll = 0.0;
    getMinMaxDepth(minDepth, maxDepthAll);

    double bboxW = bbox3D.maxX - bbox3D.minX;
    double bboxH = bbox3D.maxY - bbox3D.minY;
    double bboxZ = maxPartDepth;

    if (bboxW <= 0.000001) bboxW = 1.0;
    if (bboxH <= 0.000001) bboxH = 1.0;
    if (bboxZ <= 0.000001) bboxZ = 1.0;

    double centerX = (bbox3D.minX + bbox3D.maxX) / 2.0;
    double centerY = (bbox3D.minY + bbox3D.maxY) / 2.0;
    double centerZ = maxPartDepth / 2.0;

    double diag = std::sqrt(bboxW * bboxW + bboxH * bboxH + bboxZ * bboxZ);
    double margin = 30.0;
    double availDim = std::min(pw - 2.0 * margin, ph - 2.0 * margin);
    double baseScale = availDim / (diag * 1.2);
    double scale3D = baseScale * m_zoom3D;

    double rotXRad = m_rotX3D * M_PI / 180.0;
    double rotYRad = m_rotY3D * M_PI / 180.0;

    double cosX = std::cos(rotXRad), sinX = std::sin(rotXRad);
    double cosY = std::cos(rotYRad), sinY = std::sin(rotYRad);

    auto Project3D = [&](double x, double y, double z) -> wxPoint {
        double dx = x - centerX;
        double dy = y - centerY;
        double dz = z - centerZ;

        double x1 = dx * cosY + dz * sinY;
        double y1 = dy;
        double z1 = -dx * sinY + dz * cosY;

        double x2 = x1;
        double y2 = y1 * cosX - z1 * sinX;

        int px = (int)(pw / 2.0 + m_panX3D + x2 * scale3D);
        int py = (int)(ph / 2.0 + m_panY3D - y2 * scale3D);
        return wxPoint(px, py);
    };

    // Draw Axes first
    wxPoint origin = Project3D(centerX, centerY, 0.0);
    wxPoint xAxis = Project3D(centerX + bboxW * 0.3, centerY, 0.0);
    wxPoint yAxis = Project3D(centerX, centerY + bboxH * 0.3, 0.0);
    wxPoint zAxis = Project3D(centerX, centerY, maxPartDepth * 0.5);

    dc.SetPen(wxPen(wxColour(220, 100, 100), 1, wxPENSTYLE_DOT));
    dc.DrawLine(origin, xAxis);
    dc.SetPen(wxPen(wxColour(100, 200, 100), 1, wxPENSTYLE_DOT));
    dc.DrawLine(origin, yAxis);
    dc.SetPen(wxPen(wxColour(100, 100, 220), 1, wxPENSTYLE_DOT));
    dc.DrawLine(origin, zAxis);

    // --- 1. SOLID FACES GENERATION AND RENDERING ---
    // World light direction vector (upper-right-top)
    const double wlx = 0.35, wly = 0.35, wlz = 0.866; // Normalized (0.35, 0.35, 0.866)

    struct DxfPoint3D {
        double x = 0.0, y = 0.0, z = 0.0;
    };

    struct LineSegment3D {
        wxPoint p1;
        wxPoint p2;
        wxColour color;
        int penWidth = 1;

        LineSegment3D(wxPoint a, wxPoint b, wxColour c, int w = 1)
            : p1(a), p2(b), color(c), penWidth(w) {}
    };

    struct Face3D {
        std::vector<wxPoint> screenPts;
        std::vector<LineSegment3D> outlineEdges;
        double sortKey = 0.0;
        double avgZView = 0.0;
        double minZView = 0.0;
        double maxZView = 0.0;
        double nxWorld = 0.0, nyWorld = 0.0, nzWorld = 1.0;
        bool isFrontFacing = true;
    };

    auto BuildFace3D = [&](const std::vector<DxfPoint3D>& worldPts, double nxW, double nyW, double nzW) -> Face3D {
        Face3D face;
        face.nxWorld = nxW;
        face.nyWorld = nyW;
        face.nzWorld = nzW;

        size_t count = worldPts.size();
        if (count < 3) return face;

        double sumZ = 0.0;
        double sumXWorld = 0.0, sumYWorld = 0.0, sumZWorld = 0.0;
        double minZ = 1e9;
        double maxZ = -1e9;
        face.screenPts.reserve(count);

        for (size_t k = 0; k < count; ++k) {
            sumXWorld += worldPts[k].x;
            sumYWorld += worldPts[k].y;
            sumZWorld += worldPts[k].z;

            double dx = worldPts[k].x - centerX;
            double dy = worldPts[k].y - centerY;
            double dz = worldPts[k].z - centerZ;

            double x1 = dx * cosY + dz * sinY;
            double y1 = dy;
            double z1 = -dx * sinY + dz * cosY;

            double x2 = x1;
            double y2 = y1 * cosX - z1 * sinX;
            double z2 = y1 * sinX + z1 * cosX;

            int px = (int)(pw / 2.0 + m_panX3D + x2 * scale3D);
            int py = (int)(ph / 2.0 + m_panY3D - y2 * scale3D);

            face.screenPts.push_back(wxPoint(px, py));
            sumZ += z2;
            if (z2 < minZ) minZ = z2;
            if (z2 > maxZ) maxZ = z2;
        }
        face.avgZView = sumZ / count;
        face.minZView = minZ;
        face.maxZView = maxZ;

        double V_z = cosY * cosX;
        double V_x = -sinY * cosX;
        double V_y = sinX;

        double cdz = (sumZWorld / count) - centerZ;
        double cdx = (sumXWorld / count) - centerX;
        double cdy = (sumYWorld / count) - centerY;

        face.sortKey = cdz * V_z + 0.02 * (cdx * V_x + cdy * V_y);

        double ny1 = nyW;
        double nz1 = -nxW * sinY + nzW * cosY;
        double nz2 = ny1 * sinX + nz1 * cosX;

        face.isFrontFacing = (nz2 > -0.01);

        return face;
    };

    struct ShapePath {
        std::vector<DxfPoint> pts;
        bool isClosed = false;
    };

    struct BuiltShape3D {
        int shapeIndex = 0;
        double depth = 0.0;
        wxColour highlightCol;
        std::vector<ShapePath> paths;
        std::vector<Face3D> sideFaces;
        std::vector<Face3D> topCapFaces;
        std::vector<Face3D> bottomCapFaces;
    };

    std::vector<BuiltShape3D> builtShapes;
    std::vector<Face3D> allRenderFaces;

    for (size_t sIdx = 0; sIdx < part.shapes.size(); ++sIdx) {
        const auto& shape = part.shapes[sIdx];
        wxColour shapeCol = getDepthColor(shape.depth, minDepth, maxDepthAll);

        BuiltShape3D bShape;
        bShape.shapeIndex = (int)sIdx;
        bShape.depth = shape.depth;
        bShape.highlightCol = shapeCol;

        std::vector<ShapePath> rawPaths;

        for (size_t idx : shape.entityIndices) {
            if (idx >= m_dxfModel.entities.size()) continue;
            const auto& entity = m_dxfModel.entities[idx];
            const DxfLayer* layer = m_dxfModel.findLayer(entity.layerName);
            if (layer && !layer->visible) continue;

            std::vector<DxfPoint> geomPoints;
            if (entity.type == DxfEntityType::LINE && entity.points.size() >= 2) {
                geomPoints = { entity.points[0], entity.points[1] };
            } else if ((entity.type == DxfEntityType::POLYLINE || entity.type == DxfEntityType::SPLINE) && entity.points.size() >= 2) {
                geomPoints = entity.points;
            } else if (entity.type == DxfEntityType::CIRCLE) {
                int steps = 32;
                for (int i = 0; i < steps; ++i) {
                    double t = 2.0 * M_PI * i / steps;
                    geomPoints.push_back(DxfPoint(entity.center.x + entity.radius * std::cos(t),
                                                  entity.center.y + entity.radius * std::sin(t)));
                }
            } else if (entity.type == DxfEntityType::ARC) {
                double startRad = entity.startAngle * M_PI / 180.0;
                double endRad = entity.endAngle * M_PI / 180.0;
                if (endRad <= startRad) endRad += 2.0 * M_PI;
                int steps = std::max(8, (int)(std::abs(endRad - startRad) / (M_PI / 36.0)));
                for (int i = 0; i <= steps; ++i) {
                    double t = startRad + (endRad - startRad) * i / steps;
                    geomPoints.push_back(DxfPoint(entity.center.x + entity.radius * std::cos(t),
                                                  entity.center.y + entity.radius * std::sin(t)));
                }
            } else if (entity.type == DxfEntityType::ELLIPSE) {
                int steps = 36;
                double startP = entity.startParam;
                double endP = entity.endParam;
                if (endP <= startP) endP += 2.0 * M_PI;
                for (int i = 0; i <= steps; ++i) {
                    double t = startP + (endP - startP) * i / steps;
                    double ex = entity.center.x + entity.majorAxis.x * std::cos(t) - entity.majorAxis.y * entity.ratio * std::sin(t);
                    double ey = entity.center.y + entity.majorAxis.y * std::cos(t) + entity.majorAxis.x * entity.ratio * std::sin(t);
                    geomPoints.push_back(DxfPoint(ex, ey));
                }
            }

            if (geomPoints.size() < 2) continue;

            bool closed = entity.isClosed || entity.type == DxfEntityType::CIRCLE || entity.type == DxfEntityType::ELLIPSE;
            double distSq = (geomPoints.front().x - geomPoints.back().x)*(geomPoints.front().x - geomPoints.back().x) +
                            (geomPoints.front().y - geomPoints.back().y)*(geomPoints.front().y - geomPoints.back().y);
            if (distSq < 1e-6) {
                closed = true;
            }

            if (closed && geomPoints.size() > 2 && distSq < 1e-6) {
                geomPoints.pop_back();
            }

            rawPaths.push_back({ geomPoints, closed });
        }

        std::vector<ShapePath> finalPaths;
        std::vector<ShapePath> openPaths;

        for (auto& path : rawPaths) {
            if (path.isClosed) {
                finalPaths.push_back(path);
            } else {
                openPaths.push_back(path);
            }
        }

        while (!openPaths.empty()) {
            ShapePath current = openPaths.back();
            openPaths.pop_back();

            bool joined = true;
            while (joined && !current.isClosed) {
                joined = false;
                for (size_t i = 0; i < openPaths.size(); ++i) {
                    auto& candidate = openPaths[i];
                    double d1 = std::hypot(current.pts.back().x - candidate.pts.front().x, current.pts.back().y - candidate.pts.front().y);
                    double d2 = std::hypot(current.pts.back().x - candidate.pts.back().x, current.pts.back().y - candidate.pts.back().y);
                    double d3 = std::hypot(current.pts.front().x - candidate.pts.back().x, current.pts.front().y - candidate.pts.back().y);
                    double d4 = std::hypot(current.pts.front().x - candidate.pts.front().x, current.pts.front().y - candidate.pts.front().y);

                    double tol = 1e-3;
                    if (d1 < tol) {
                        current.pts.insert(current.pts.end(), candidate.pts.begin() + 1, candidate.pts.end());
                        openPaths.erase(openPaths.begin() + i);
                        joined = true;
                        break;
                    } else if (d2 < tol) {
                        std::vector<DxfPoint> rev(candidate.pts.rbegin(), candidate.pts.rend());
                        current.pts.insert(current.pts.end(), rev.begin() + 1, rev.end());
                        openPaths.erase(openPaths.begin() + i);
                        joined = true;
                        break;
                    } else if (d3 < tol) {
                        current.pts.insert(current.pts.begin(), candidate.pts.begin(), candidate.pts.end() - 1);
                        openPaths.erase(openPaths.begin() + i);
                        joined = true;
                        break;
                    } else if (d4 < tol) {
                        std::vector<DxfPoint> rev(candidate.pts.rbegin(), candidate.pts.rend());
                        current.pts.insert(current.pts.begin(), rev.begin(), rev.end() - 1);
                        openPaths.erase(openPaths.begin() + i);
                        joined = true;
                        break;
                    }
                }

                double closeDist = std::hypot(current.pts.front().x - current.pts.back().x, current.pts.front().y - current.pts.back().y);
                if (current.pts.size() >= 3 && closeDist < 1e-3) {
                    current.isClosed = true;
                    current.pts.pop_back();
                    break;
                }
            }

            finalPaths.push_back(current);
        }

        bShape.paths = finalPaths;
        builtShapes.push_back(bShape);
    }

    // Collect 2D closed boundaries of all shapes for CSG 3D wall splitting & base culling
    struct Shape2DInfo {
        size_t shapeIndex;
        double depth;
        std::vector<DxfPoint> pts;
    };

    std::vector<Shape2DInfo> closedShapes2D;
    for (size_t sIdx = 0; sIdx < builtShapes.size(); ++sIdx) {
        for (const auto& path : builtShapes[sIdx].paths) {
            if (path.isClosed && path.pts.size() >= 3) {
                std::vector<DxfPoint> pts = path.pts;
                double signedArea = 0.0;
                size_t n = pts.size();
                for (size_t i = 0; i < n; ++i) {
                    size_t nextIdx = (i + 1) % n;
                    signedArea += (pts[i].x * pts[nextIdx].y - pts[nextIdx].x * pts[i].y);
                }
                if (signedArea < 0.0) {
                    std::reverse(pts.begin(), pts.end());
                }
                closedShapes2D.push_back({ sIdx, builtShapes[sIdx].depth, pts });
            }
        }
    }

    auto IsPointInPolygon = [](double px, double py, const std::vector<DxfPoint>& pts) -> bool {
        bool inside = false;
        size_t n = pts.size();
        if (n < 3) return false;
        for (size_t i = 0, j = n - 1; i < n; j = i++) {
            double xi = pts[i].x, yi = pts[i].y;
            double xj = pts[j].x, yj = pts[j].y;
            bool intersect = ((yi > py) != (yj > py)) &&
                             (px < (xj - xi) * (py - yi) / (yj - yi + 1e-12) + xi);
            if (intersect) inside = !inside;
        }
        return inside;
    };

    // Build 3D Faces with CSG Wall Splitting & Coplanar Base Culling
    for (size_t sIdx = 0; sIdx < builtShapes.size(); ++sIdx) {
        auto& bShape = builtShapes[sIdx];
        double selfDepth = bShape.depth;
        wxColour shapeCol = bShape.highlightCol;
        bool isSelectedShape = (m_selectedShapeInPartIndex == (int)sIdx);
        int topWidth = isSelectedShape ? 3 : 2;
        wxColour bottomCol(std::max(0, shapeCol.Red() - 80),
                           std::max(0, shapeCol.Green() - 80),
                           std::max(0, shapeCol.Blue() - 80));

        for (auto path : bShape.paths) {
            size_t n = path.pts.size();
            if (n < 2) continue;

            // Ensure closed paths are oriented CCW (Counter-Clockwise)
            if (path.isClosed && n >= 3) {
                double signedArea = 0.0;
                for (size_t i = 0; i < n; ++i) {
                    size_t nextIdx = (i + 1) % n;
                    signedArea += (path.pts[i].x * path.pts[nextIdx].y - path.pts[nextIdx].x * path.pts[i].y);
                }
                if (signedArea < 0.0) {
                    std::reverse(path.pts.begin(), path.pts.end());
                }
            }

            // Side Quads
            size_t numSegments = path.isClosed ? n : (n - 1);
            for (size_t i = 0; i < numSegments; ++i) {
                size_t nextIdx = (i + 1) % n;
                const auto& p1 = path.pts[i];
                const auto& p2 = path.pts[nextIdx];

                // Outward normal in 2D
                double segDx = p2.x - p1.x;
                double segDy = p2.y - p1.y;
                double segLen = std::hypot(segDx, segDy);
                double nxW = 0.0, nyW = 0.0;
                if (segLen > 1e-9) {
                    nxW = segDy / segLen;
                    nyW = -segDx / segLen;
                }

                // Check 2D segment midpoint against all other shapes
                double mx = (p1.x + p2.x) / 2.0;
                double my = (p1.y + p2.y) / 2.0;

                double maxOccludingDepth = 0.0;
                bool isInsideAnotherBase = false;
                for (const auto& other : closedShapes2D) {
                    if (other.shapeIndex == sIdx) continue;
                    if (IsPointInPolygon(mx, my, other.pts)) {
                        isInsideAnotherBase = true;
                        if (other.depth > maxOccludingDepth) {
                            maxOccludingDepth = other.depth;
                        }
                    }
                }

                double zStart = 0.0;
                if (isInsideAnotherBase && maxOccludingDepth > 0.0) {
                    zStart = maxOccludingDepth;
                }

                if (selfDepth > zStart + 1e-4) {
                    // Quad from zStart to selfDepth (exposed portion above any occluding body)
                    std::vector<DxfPoint3D> quad1 = {
                        { p1.x, p1.y, zStart },
                        { p2.x, p2.y, zStart },
                        { p2.x, p2.y, selfDepth },
                        { p1.x, p1.y, selfDepth }
                    };

                    Face3D f1 = BuildFace3D(quad1, nxW, nyW, 0.0);
                    if (f1.isFrontFacing && f1.screenPts.size() == 4) {
                        f1.outlineEdges.push_back({ f1.screenPts[3], f1.screenPts[2], shapeCol, topWidth });
                        if (zStart < 1e-4 && !isInsideAnotherBase) {
                            f1.outlineEdges.push_back({ f1.screenPts[0], f1.screenPts[1], bottomCol, 1 });
                        }
                        f1.outlineEdges.push_back({ f1.screenPts[0], f1.screenPts[3], wxColour(120, 125, 135), 1 });
                        f1.outlineEdges.push_back({ f1.screenPts[1], f1.screenPts[2], wxColour(120, 125, 135), 1 });
                        bShape.sideFaces.push_back(f1);
                        allRenderFaces.push_back(f1);
                    }

                    // Reverse Quad for open paths
                    if (!path.isClosed) {
                        std::vector<DxfPoint3D> quad2 = {
                            { p1.x, p1.y, selfDepth },
                            { p2.x, p2.y, selfDepth },
                            { p2.x, p2.y, zStart },
                            { p1.x, p1.y, zStart }
                        };
                        Face3D f2 = BuildFace3D(quad2, -nxW, -nyW, 0.0);
                        if (f2.isFrontFacing && f2.screenPts.size() == 4) {
                            f2.outlineEdges.push_back({ f2.screenPts[0], f2.screenPts[1], shapeCol, topWidth });
                            if (zStart < 1e-4 && !isInsideAnotherBase) {
                                f2.outlineEdges.push_back({ f2.screenPts[3], f2.screenPts[2], bottomCol, 1 });
                            }
                            f2.outlineEdges.push_back({ f2.screenPts[0], f2.screenPts[3], wxColour(120, 125, 135), 1 });
                            f2.outlineEdges.push_back({ f2.screenPts[1], f2.screenPts[2], wxColour(120, 125, 135), 1 });
                            bShape.sideFaces.push_back(f2);
                            allRenderFaces.push_back(f2);
                        }
                    }
                }
            }

            // Top Cap (+Z normal at Z = selfDepth)
            if (path.isClosed && n >= 3) {
                std::vector<DxfPoint3D> topCap(n);
                for (size_t k = 0; k < n; ++k) {
                    topCap[k] = { path.pts[k].x, path.pts[k].y, selfDepth };
                }
                Face3D fTop = BuildFace3D(topCap, 0.0, 0.0, 1.0);
                if (fTop.isFrontFacing && fTop.screenPts.size() >= 3) {
                    size_t count = fTop.screenPts.size();
                    for (size_t k = 0; k < count; ++k) {
                        size_t nextK = (k + 1) % count;
                        fTop.outlineEdges.push_back({ fTop.screenPts[k], fTop.screenPts[nextK], shapeCol, topWidth });
                    }
                    bShape.topCapFaces.push_back(fTop);
                    allRenderFaces.push_back(fTop);
                }

                // Bottom Cap (-Z normal at Z = 0.0)
                std::vector<DxfPoint3D> bottomCap(n);
                for (size_t k = 0; k < n; ++k) {
                    bottomCap[k] = { path.pts[n - 1 - k].x, path.pts[n - 1 - k].y, 0.0 };
                }
                Face3D fBottom = BuildFace3D(bottomCap, 0.0, 0.0, -1.0);
                if (fBottom.isFrontFacing && fBottom.screenPts.size() >= 3) {
                    size_t count = fBottom.screenPts.size();
                    for (size_t k = 0; k < count; ++k) {
                        size_t nextK = (k + 1) % count;
                        fBottom.outlineEdges.push_back({ fBottom.screenPts[k], fBottom.screenPts[nextK], bottomCol, 1 });
                    }
                    bShape.bottomCapFaces.push_back(fBottom);
                    allRenderFaces.push_back(fBottom);
                }
            }
        }
    }

    // Depth sort front-facing faces back-to-front with Line-of-Sight distance key
    std::sort(allRenderFaces.begin(), allRenderFaces.end(), [](const Face3D& a, const Face3D& b) {
        if (std::abs(a.sortKey - b.sortKey) > 1e-4) {
            return a.sortKey < b.sortKey;
        }
        if (std::abs(a.avgZView - b.avgZView) > 1e-3) {
            return a.avgZView < b.avgZView;
        }
        if (std::abs(a.maxZView - b.maxZView) > 1e-3) {
            return a.maxZView < b.maxZView;
        }
        return a.minZView < b.minZView;
    });

    // Render solid faces back-to-front with metallic CAD shading
    const wxColour baseGray(195, 200, 208);

    for (const auto& face : allRenderFaces) {
        // World-space diffuse lighting for crisp, rich CAD surface tones
        double dot = face.nxWorld * wlx + face.nyWorld * wly + face.nzWorld * wlz;
        double diffuse = std::max(0.0, dot);

        // Transform normal to view space for specular highlights
        double nx1 = face.nxWorld * cosY + face.nzWorld * sinY;
        double ny1 = face.nyWorld;
        double nz1 = -face.nxWorld * sinY + face.nzWorld * cosY;

        double nx2 = nx1;
        double ny2 = ny1 * cosX - nz1 * sinX;
        double nz2 = ny1 * sinX + nz1 * cosX;

        // Specular highlight
        double spec = 0.0;
        if (diffuse > 0.0) {
            double hx = 0.4082, hy = 0.4082, hz = 1.8165;
            double hLen = std::sqrt(hx*hx + hy*hy + hz*hz);
            if (hLen > 1e-9) {
                double NdotH = std::max(0.0, (nx2 * hx + ny2 * hy + nz2 * hz) / hLen);
                spec = std::pow(NdotH, 24) * 0.15;
            }
        }

        double ambient = 0.32;
        double intensity = std::min(1.0, ambient + 0.58 * diffuse);

        int r = std::max(0, std::min(255, (int)(baseGray.Red() * intensity + spec * 255)));
        int g = std::max(0, std::min(255, (int)(baseGray.Green() * intensity + spec * 255)));
        int b = std::max(0, std::min(255, (int)(baseGray.Blue() * intensity + spec * 255)));

        wxColour faceColour(r, g, b);
        dc.SetPen(wxPen(faceColour, 1, wxPENSTYLE_SOLID));
        dc.SetBrush(wxBrush(faceColour, wxBRUSHSTYLE_SOLID));
        dc.DrawPolygon((int)face.screenPts.size(), face.screenPts.data());

        // Render face outline edges immediately over the solid polygon
        for (const auto& edge : face.outlineEdges) {
            dc.SetPen(wxPen(edge.color, edge.penWidth, wxPENSTYLE_SOLID));
            dc.DrawLine(edge.p1, edge.p2);
        }
    }
}

void BendiCAMFrame::OnTimerTrigger(wxTimerEvent& event)
{
}

WXLRESULT BendiCAMFrame::MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam) {
    if (message == WM_INPUT) {
        UINT dwSize;
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));

        LPBYTE lpb = new BYTE[dwSize];
        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) == dwSize) {
            RAWINPUT* raw = (RAWINPUT*)lpb;
            if (raw->header.dwType == RIM_TYPEHID) {
                if (preparsedData == nullptr) {
                    UINT dataSize = 0;
                    GetRawInputDeviceInfo(raw->header.hDevice, RIDI_PREPARSEDDATA, NULL, &dataSize);
                    preparsedData = (PHIDP_PREPARSED_DATA)malloc(dataSize);
                    GetRawInputDeviceInfo(raw->header.hDevice, RIDI_PREPARSEDDATA, preparsedData, &dataSize);

                    HIDP_CAPS caps;
                    HidP_GetCaps(preparsedData, &caps);
                }
            }
        }
        delete[] lpb;
        return 0;
    }
    return wxFrame::MSWWindowProc(message, wParam, lParam);
}

void BendiCAMFrame::UpdateWindowTitle()
{
    wxString title;
    if (!m_loadedFileName.IsEmpty()) {
        title = m_loadedFileName;
    } else if (!m_currentDxfPath.empty()) {
        wxFileName fn(wxString::FromUTF8(m_currentDxfPath.c_str()));
        title = fn.GetFullName();
    } else {
        title = _T("drawing.dxf");
    }

    if (m_isDirty) {
        title += _T(" *");
    }
    title += _T(" - BendiCAM");
    SetTitle(title);
}

void BendiCAMFrame::MarkDirty()
{
    if (!m_isDirty) {
        m_isDirty = true;
        UpdateWindowTitle();
    }
}

void BendiCAMFrame::MarkClean()
{
    m_isDirty = false;
    UpdateWindowTitle();
}

void BendiCAMFrame::OnToolOrMaterialSettingChange(wxCommandEvent& event)
{
    MarkDirty();
    if (PanelDrawing) PanelDrawing->Refresh();
}

void BendiCAMFrame::AutoCreatePartsFromDxf()
{
    if (!m_parts.empty() || m_dxfModel.entities.empty()) return;

    double connTol = 0.1;
    if (TextCtrlConnTolerance) TextCtrlConnTolerance->GetValue().ToDouble(&connTol);
    bool closeCoinciding = CheckBoxClosePolylines ? CheckBoxClosePolylines->IsChecked() : true;

    double defaultDepth = 5.0;
    if (TextCtrlDepth) TextCtrlDepth->GetValue().ToDouble(&defaultDepth);

    std::set<size_t> assignedIndices;
    size_t numEntities = m_dxfModel.entities.size();

    for (size_t i = 0; i < numEntities; ++i) {
        if (assignedIndices.count(i)) continue;

        std::set<size_t> connected = m_dxfModel.findConnectedEntities(i, connTol, closeCoinciding);
        if (connected.empty()) continue;

        CamPart part;
        part.id = m_nextPartId++;

        CamShape shape;
        shape.shapeId = 1;
        shape.depth = defaultDepth;
        for (size_t idx : connected) {
            shape.entityIndices.insert(idx);
            assignedIndices.insert(idx);
        }

        part.shapes.push_back(shape);
        part.name = wxString::Format(_("Part %d (%zu shape(s))"), part.id, part.shapes.size()).ToStdString();
        m_parts.push_back(part);
    }

    RebuildPartsTree();
    UpdatePartButtonStates();
}

bool BendiCAMFrame::ExportGCodeToFile(const wxString& filePath)
{
    if (m_parts.empty()) {
        AutoCreatePartsFromDxf();
    }

    if (m_parts.empty()) {
        TextCtrlStatus->AppendText(_("No parts defined or auto-detected. Unable to generate G-Code.\n"));
        return false;
    }

    GenerateGCode(filePath);
    return true;
}

void BendiCAMFrame::OnExportGCode(wxCommandEvent& event)
{
    if (m_parts.empty()) {
        AutoCreatePartsFromDxf();
    }

    if (m_parts.empty()) {
        wxMessageBox(_("No parts defined. Please create parts before exporting G-Code."), _("Export G-Code"), wxOK | wxICON_INFORMATION, this);
        return;
    }

    wxString defaultName = _T("output.cnc");
    if (!m_currentProjectPath.empty()) {
        wxFileName fn(wxString::FromUTF8(m_currentProjectPath.c_str()));
        defaultName = fn.GetName() + _T(".cnc");
    } else if (!m_currentDxfPath.empty()) {
        wxFileName fn(wxString::FromUTF8(m_currentDxfPath.c_str()));
        defaultName = fn.GetName() + _T(".cnc");
    }

    wxFileDialog saveFileDialog(this, _("Export EdingCNC G-Code"), "", defaultName,
                                _("CNC Files (*.cnc)|*.cnc|GCode Files (*.gcode;*.nc)|*.gcode;*.nc|All Files (*.*)|*.*"),
                                wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (saveFileDialog.ShowModal() == wxID_CANCEL)
        return;

    ExportGCodeToFile(saveFileDialog.GetPath());
}

void BendiCAMFrame::GenerateGCode(const wxString& filePath)
{
    std::ofstream out(filePath.ToStdString());
    if (!out.is_open()) {
        TextCtrlStatus->AppendText(wxString::Format(_("Failed to open G-code output file: %s\n"), filePath));
        return;
    }

    double diameter = 2.0, rpm = 12000.0, feedXY = 500.0, feedZ = 250.0, stepdown = 1.0, stepover = 70.0, rampAngle = 20.0;
    double topHeight = 10.0, safeZ = 10.0;

    if (TextCtrlToolDiameter) TextCtrlToolDiameter->GetValue().ToDouble(&diameter);
    if (TextCtrlToolRPM) TextCtrlToolRPM->GetValue().ToDouble(&rpm);
    if (TextCtrlToolFeedXY) TextCtrlToolFeedXY->GetValue().ToDouble(&feedXY);
    if (TextCtrlToolFeedZ) TextCtrlToolFeedZ->GetValue().ToDouble(&feedZ);
    if (TextCtrlToolStepdown) TextCtrlToolStepdown->GetValue().ToDouble(&stepdown);
    if (TextCtrlToolStepover) TextCtrlToolStepover->GetValue().ToDouble(&stepover);
    if (TextCtrlToolRampAngle) TextCtrlToolRampAngle->GetValue().ToDouble(&rampAngle);
    if (TextCtrlMatTopHeight) TextCtrlMatTopHeight->GetValue().ToDouble(&topHeight);
    if (TextCtrlSafeZ) TextCtrlSafeZ->GetValue().ToDouble(&safeZ);

    double retractZ = topHeight + safeZ;
    char buf[512];

    std::vector<std::string> allHeaderWarnings;
    for (const auto& part : m_parts) {
        auto valRes = validatePartHierarchy(m_dxfModel, part.shapes, topHeight);
        if (valRes.hasViolation) {
            for (const auto& warnMsg : valRes.warnings) {
                allHeaderWarnings.push_back(warnMsg);
                TextCtrlStatus->AppendText(wxString::Format(_("Warning: %s\n"), wxString::FromUTF8(warnMsg.c_str())));
                std::cerr << "Warning: " << warnMsg << std::endl;
            }
        }
    }

    // Header
    out << "( BendiCAM EdingCNC G-Code Export )\n";
    wxFileName fnDxf(wxString::FromUTF8(m_currentDxfPath.c_str()));
    out << "( Source Drawing: " << fnDxf.GetFullName().ToStdString() << " )\n";
    snprintf(buf, sizeof(buf), "( Tool Diameter: %.3f mm )\n", diameter); out << buf;
    snprintf(buf, sizeof(buf), "( Spindle RPM: %.3f min^-1 )\n", rpm); out << buf;
    snprintf(buf, sizeof(buf), "( Feed XY: %.3f mm/min )\n", feedXY); out << buf;
    snprintf(buf, sizeof(buf), "( Feed Z: %.3f mm/min )\n", feedZ); out << buf;
    snprintf(buf, sizeof(buf), "( Stepdown: %.3f mm )\n", stepdown); out << buf;
    snprintf(buf, sizeof(buf), "( Stepover: %.3f %% )\n", stepover); out << buf;
    snprintf(buf, sizeof(buf), "( Ramp Angle: %.3f deg )\n", rampAngle); out << buf;
    snprintf(buf, sizeof(buf), "( Material Top Height: %.3f mm )\n", topHeight); out << buf;
    snprintf(buf, sizeof(buf), "( Safe Z: %.3f mm )\n", safeZ); out << buf;

    for (const auto& wMsg : allHeaderWarnings) {
        out << "( WARNING: Containment hierarchy violation detected - " << wMsg << " )\n";
    }

    out << "%\n";
    out << "G21 ( Metric units )\n";
    out << "G90 ( Absolute positioning )\n";
    out << "G17 ( XY plane )\n";
    out << "G94 ( Feed rate per minute )\n";
    out << "G40 ( Cancel cutter compensation )\n";
    out << "G49 ( Cancel tool length offset )\n";
    out << "G80 ( Cancel canned cycles )\n\n";

    snprintf(buf, sizeof(buf), "T1 M6 ( Select Tool 1 )\n"); out << buf;
    snprintf(buf, sizeof(buf), "S%.0f M3 ( Spindle ON CW )\n", rpm); out << buf;
    out << "G04 P1.0 ( Pause 1s for spindle spin-up )\n\n";

    snprintf(buf, sizeof(buf), "G0 Z%.3f\n\n", retractZ); out << buf;

    DxfPoint currentToolPos(0, 0);
    bool isRetracted = true;

    for (const auto& part : m_parts) {
        out << "( --- Part " << part.id << ": " << part.name << " --- )\n";

        auto heightBands = computePartHeightBands(part.shapes, topHeight);

        for (const auto& band : heightBands) {
            double zUpper = band.zUpper;
            double zFloor = band.zFloor;

            snprintf(buf, sizeof(buf), "( Clear milling from Z = %.3f mm down to Z = %.3f mm )\n", zUpper, zFloor);
            out << buf;

            double heightDiff = zUpper - zFloor;
            int numPasses = (int)std::ceil(heightDiff / stepdown);
            if (numPasses < 1) numPasses = 1;

            std::vector<double> passZLevels;
            for (int k = 1; k <= numPasses; ++k) {
                double pZ = zUpper - k * (heightDiff / numPasses);
                passZLevels.push_back(pZ);
            }

            auto bandLoops = generateBandClearingLoops(m_dxfModel, part.shapes, zUpper, zFloor, topHeight, diameter, stepover);

            for (int pIdx = 0; pIdx < numPasses; ++pIdx) {
                double curZ = passZLevels[pIdx];
                double prevZ = (pIdx == 0) ? zUpper : passZLevels[pIdx - 1];

                snprintf(buf, sizeof(buf), "( Pass %d/%d at Z = %.3f mm )\n", pIdx + 1, numPasses, curZ);
                out << buf;

                for (size_t loopIdx = 0; loopIdx < bandLoops.size(); ++loopIdx) {
                    const auto& loop = bandLoops[loopIdx];
                    bool isRamping = (loopIdx == 0);
                    emitAnalyticalLoopGCode(out, loop, prevZ, curZ, feedZ, feedXY, retractZ, rampAngle, isRamping, currentToolPos, isRetracted);
                }
            }
        }

        auto cutoutPaths = generateOuterCutoutPaths(m_dxfModel, part.shapes, diameter);
        if (!cutoutPaths.empty()) {
            double targetProfileZ = 0.0;
            snprintf(buf, sizeof(buf), "( Outer Profile Contour Milling around Part down to Z = %.3f mm )\n", targetProfileZ);
            out << buf;

            int numCutoutPasses = (int)std::ceil(topHeight / stepdown);
            if (numCutoutPasses < 1) numCutoutPasses = 1;

            std::vector<double> cutoutZLevels;
            for (int k = 1; k <= numCutoutPasses; ++k) {
                double pZ = topHeight - k * (topHeight / numCutoutPasses);
                cutoutZLevels.push_back(pZ);
            }

            for (int pIdx = 0; pIdx < numCutoutPasses; ++pIdx) {
                double curZ = cutoutZLevels[pIdx];
                double prevZ = (pIdx == 0) ? topHeight : cutoutZLevels[pIdx - 1];

                snprintf(buf, sizeof(buf), "( Profile Pass %d/%d at Z = %.3f mm )\n", pIdx + 1, numCutoutPasses, curZ);
                out << buf;

                for (const auto& contourPath : cutoutPaths) {
                    emitAnalyticalLoopGCode(out, contourPath, prevZ, curZ, feedZ, feedXY, retractZ, rampAngle, true, currentToolPos, isRetracted);
                }
            }
        }

        isRetracted = true;
    }

    out << "\n( End of Job )\n";
    out << "M5 ( Spindle OFF )\n";
    snprintf(buf, sizeof(buf), "G0 Z%.3f ( Retract to Safe Z )\n", retractZ); out << buf;
    out << "M30 ( Program End )\n";
    out << "%\n";

    out.close();
    TextCtrlStatus->AppendText(wxString::Format(_("Exported EdingCNC G-code: %s\n"), filePath));
}

void BendiCAMFrame::OnQuit(wxCloseEvent& event)
{
    Close(false);
}

void BendiCAMFrame::OnClose(wxCloseEvent& event)
{
    if (m_isDirty) {
        int res = wxMessageBox(_("The project has unsaved changes. Do you want to save them before closing?"),
                               _("Unsaved Changes"),
                               wxYES_NO | wxCANCEL | wxICON_QUESTION, this);
        if (res == wxYES) {
            wxCommandEvent dummyEvt;
            OnSaveProject(dummyEvt);
            if (m_isDirty) {
                if (event.CanVeto()) event.Veto();
                return;
            }
        } else if (res == wxCANCEL) {
            if (event.CanVeto()) event.Veto();
            return;
        }
    }
    Destroy();
}
