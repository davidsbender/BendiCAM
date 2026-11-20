/***************************************************************
 * Name:      BendiCAMApp.cpp
 * Purpose:   Code for Application Class
 * Author:    David Bender (d_bender@bluewin.ch)
 * Created:   2026-02-12
 * Copyright: David Bender ()
 * License:
 **************************************************************/

#include "BendiCAMApp.h"

//(*AppHeaders
#include "BendiCAMMain.h"
#include <wx/image.h>
//*)

IMPLEMENT_APP(BendiCAMApp);

bool BendiCAMApp::OnInit()
{
    //(*AppInitialize
    bool wxsOK = true;
    wxInitAllImageHandlers();
    if ( wxsOK )
    {
        BendiCAMFrame* Frame = new BendiCAMFrame(0);
        Frame->Show();
        SetTopWindow(Frame);
    }
    //*)
    return wxsOK;

}
