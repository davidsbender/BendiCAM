/***************************************************************
 * Name:      BendiCAMApp.cpp
 * Purpose:   Code for Application Class
 * Author:    David Bender (d_bender@bluewin.ch)
 * Created:   2026-02-12
 * Copyright: David Bender ()
 * License:
 **************************************************************/

#include "BendiCAMApp.h"
#include "BendiCAMMain.h"
#include <wx/image.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <iostream>

IMPLEMENT_APP(BendiCAMApp);

bool BendiCAMApp::OnInit()
{
    wxInitAllImageHandlers();

    wxString inputFile;
    wxString outputFile;
    bool generateCnc = false;
    bool showHelp = false;

    for (int i = 1; i < argc; ++i) {
        wxString arg = argv[i];
        if (arg == _T("-h") || arg == _T("--help") || arg == _T("/?")) {
            showHelp = true;
        } else if (arg == _T("-c") || arg == _T("--cnc") || arg == _T("-g") || arg == _T("--generate-cnc")) {
            generateCnc = true;
            if (i + 1 < argc) {
                wxString nextArg = argv[i + 1];
                if (!nextArg.StartsWith(_T("-"))) {
                    outputFile = nextArg;
                    i++;
                }
            }
        } else if (arg == _T("-o") || arg == _T("--output")) {
            generateCnc = true;
            if (i + 1 < argc) {
                outputFile = argv[i + 1];
                i++;
            }
        } else if (arg.StartsWith(_T("--cnc=")) || arg.StartsWith(_T("--generate-cnc=")) || arg.StartsWith(_T("--output="))) {
            generateCnc = true;
            outputFile = arg.AfterFirst(_T('='));
        } else if (!arg.StartsWith(_T("-"))) {
            if (inputFile.empty()) {
                inputFile = arg;
            } else if (outputFile.empty() && generateCnc) {
                outputFile = arg;
            }
        }
    }

    if (showHelp) {
        wxString usage = _T("BendiCAM CAD/CAM Toolpath Generator\n\n")
                         _T("Usage:\n")
                         _T("  BendiCAM.exe [options] [file]\n\n")
                         _T("Arguments:\n")
                         _T("  <file.dxf>             Open CAD drawing file\n")
                         _T("  <file.bcam>            Open BendiCAM project file\n\n")
                         _T("Options:\n")
                         _T("  -c, --cnc [output.cnc] Generate .cnc G-code file immediately\n")
                         _T("  -g, --generate-cnc     Same as --cnc\n")
                         _T("  -o, --output <file>    Specify output .cnc filename\n")
                         _T("  -h, --help             Show this help message\n");
        std::cout << usage.ToStdString();
        wxMessageBox(usage, _T("BendiCAM Command Line Usage"), wxOK | wxICON_INFORMATION);
        return false;
    }

    BendiCAMFrame* Frame = new BendiCAMFrame(0);
    SetTopWindow(Frame);

    if (!inputFile.empty()) {
        wxFileName fn(inputFile);
        wxString ext = fn.GetExt().Lower();

        bool loaded = false;
        if (ext == _T("dxf")) {
            loaded = Frame->OpenDxf(inputFile);
        } else if (ext == _T("bcam") || ext == _T("json")) {
            loaded = Frame->OpenProject(inputFile);
        } else {
            loaded = Frame->OpenProject(inputFile);
            if (!loaded) {
                loaded = Frame->OpenDxf(inputFile);
            }
        }

        if (loaded) {
            std::cout << "Successfully opened file: " << inputFile.ToStdString() << std::endl;
        } else {
            std::cerr << "Error: Failed to open file: " << inputFile.ToStdString() << std::endl;
            if (generateCnc) {
                delete Frame;
                return false;
            }
        }

        if (generateCnc && loaded) {
            if (outputFile.empty()) {
                outputFile = fn.GetPathWithSep() + fn.GetName() + _T(".cnc");
            }

            bool generated = Frame->ExportGCodeToFile(outputFile);
            if (generated) {
                std::cout << "Successfully generated G-code: " << outputFile.ToStdString() << std::endl;
            } else {
                std::cerr << "Error: Failed to generate G-code to " << outputFile.ToStdString() << std::endl;
            }

            delete Frame;
            return false;
        }
    }

    Frame->Show();
    return true;
}
