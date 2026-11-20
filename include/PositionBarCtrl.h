#ifndef POSITIONBARCTRL_H
#define POSITIONBARCTRL_H

#include <wx/window.h>
#include <wx/dcclient.h>
#include <wx/dcbuffer.h>
#include <wx/brush.h>
#include <wx/pen.h>
#include <wx/math.h>

class PositionBarCtrl : public wxWindow
{
public:
    PositionBarCtrl(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, bool vertical = false)
        : wxWindow(parent, id, pos, size, wxFULL_REPAINT_ON_RESIZE), m_vertical(vertical)
    {
        m_xValue = 32768;
        SetBackgroundStyle(wxBG_STYLE_PAINT);

        Bind(wxEVT_PAINT, &PositionBarCtrl::OnPaint, this);
        Bind(wxEVT_ERASE_BACKGROUND, &PositionBarCtrl::OnEraseBackground, this);
    }

    void SetXValue(unsigned int xValue)
    {
        if (m_xValue != xValue)
        {
            m_xValue = xValue;
            Refresh();
        }
    }

private:
    unsigned int m_xValue;
    bool m_vertical;

    void OnEraseBackground(wxEraseEvent& event)
    {
        // Prevent flicker
    }

    void OnPaint(wxPaintEvent& event)
    {
        wxAutoBufferedPaintDC dc(this);

        wxSize size = GetClientSize();
        int w = size.GetWidth();
        int h = size.GetHeight();

        // 1. White background
        dc.SetPen(wxPen(wxColour(220, 220, 220), 1, wxPENSTYLE_SOLID));
        dc.SetBrush(*wxWHITE_BRUSH);
        dc.DrawRoundedRectangle(2, 2, w - 4, h - 4, 6);

        // 2. Draw tick mark and calculate dash position
        int marginLeft = 12;
        int marginRight = 12;
        int marginTop = 12;
        int marginBottom = 12;

        int trackWidth = m_vertical ? (h - marginTop - marginBottom) : (w - marginLeft - marginRight);

        int centerVal = m_vertical ? (marginTop + trackWidth / 2) : (marginLeft + trackWidth / 2);

        dc.SetPen(wxPen(wxColour(200, 200, 205), 1, wxPENSTYLE_DOT));
        if (m_vertical)
            dc.DrawLine(6, centerVal, w - 6, centerVal);
        else
            dc.DrawLine(centerVal, 6, centerVal, h - 6);

        // 3. Calculate position
        double norm = (double)m_xValue / 65535.0;
        if (norm < 0.0) norm = 0.0;
        if (norm > 1.0) norm = 1.0;

        int dashPos = (m_vertical ? marginTop : marginLeft) + (int)(norm * trackWidth);

        int dashSizeW = m_vertical ? (w - 10) : 8;
        int dashSizeH = m_vertical ? 8 : (h - 10);

        int dashX = m_vertical ? 5 : (dashPos - 4);
        int dashY = m_vertical ? (dashPos - 4) : 5;

        // 4. Draw Dash
        wxColour redColor(230, 45, 45);
        dc.SetPen(wxPen(redColor, 1, wxPENSTYLE_SOLID));
        dc.SetBrush(wxBrush(redColor));
        dc.DrawRoundedRectangle(dashX, dashY, dashSizeW, dashSizeH, 3);
    }
};

#endif // POSITIONBARCTRL_H
