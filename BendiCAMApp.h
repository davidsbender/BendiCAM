/***************************************************************
 * Name:      BendiCAMApp.h
 * Purpose:   Defines Application Class
 * Author:    David Bender (d_bender@bluewin.ch)
 * Created:   2026-02-12
 * Copyright: David Bender ()
 * License:
 **************************************************************/

#ifndef BENDICAMAPP_H
#define BENDICAMAPP_H

#include <wx/app.h>

class BendiCAMApp : public wxApp
{
    public:
        virtual bool OnInit();
};

#endif // BENDICAMAPP_H
