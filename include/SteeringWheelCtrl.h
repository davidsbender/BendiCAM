#ifndef STEERINGWHEELCTRL_H
#define STEERINGWHEELCTRL_H

#include <wx/window.h>
#include <wx/dcclient.h>
#include <wx/dcbuffer.h>
#include <wx/brush.h>
#include <wx/pen.h>
#include <wx/math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

class SteeringWheelCtrl : public wxWindow
{
public:
    SteeringWheelCtrl(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size)
        : wxWindow(parent, id, pos, size, wxFULL_REPAINT_ON_RESIZE)
    {
        m_xValue = 32768;
        m_angle = 0.0;
        SetBackgroundStyle(wxBG_STYLE_PAINT);

        Bind(wxEVT_PAINT, &SteeringWheelCtrl::OnPaint, this);
        Bind(wxEVT_ERASE_BACKGROUND, &SteeringWheelCtrl::OnEraseBackground, this);
    }

    void SetXValue(unsigned int xValue)
    {
        if (m_xValue != xValue)
        {
            m_xValue = xValue;

            // Normalize X value from [0, 65535] to [-1.0, 1.0]
            double norm = ((double)m_xValue - 32768.0) / 32768.0;
            if (norm < -1.0) norm = -1.0;
            if (norm > 1.0) norm = 1.0;

            // 5 turns total -> 2.5 turns to left (-2.5), 2.5 turns to right (+2.5)
            // 1 turn = 2 * PI radians.
            m_angle = norm * 2.5 * 2.0 * M_PI;

            Refresh();
        }
    }

private:
    unsigned int m_xValue;
    double m_angle; // in radians

    void OnEraseBackground(wxEraseEvent& event)
    {
        // Prevent flicker by doing nothing (background is painted in OnPaint via wxBufferedPaintDC)
    }

    void OnPaint(wxPaintEvent& event)
    {
        wxAutoBufferedPaintDC dc(this);

        // Fill background with the parent's background color
        wxColour bgColour = GetParent()->GetBackgroundColour();
        dc.SetBackground(wxBrush(bgColour));
        dc.Clear();

        wxSize size = GetClientSize();
        int cx = size.GetWidth() / 2;
        int cy = size.GetHeight() / 2;
        int radius = wxMin(cx, cy) - 12; // 12px margin

        if (radius < 15) return;

        // Draw a subtle outer border or background for the control area to make it look clean
        dc.SetPen(wxPen(wxColour(220, 220, 220), 1, wxPENSTYLE_SOLID));
        dc.SetBrush(wxBrush(wxColour(245, 245, 245)));
        dc.DrawRoundedRectangle(2, 2, size.GetWidth() - 4, size.GetHeight() - 4, 8);

        // Steering wheel dimensions
        double ringThickness = radius * 0.16;
        if (ringThickness < 4) ringThickness = 4;
        double hubRadius = radius * 0.24;

        // Colors
        wxColour wheelColor(45, 45, 48);        // Dark grey / charcoal for leather/rubber grip
        wxColour spokeColor(160, 160, 164);     // Silver/aluminum spokes
        wxColour stripeColor(230, 45, 45);      // Red racing stripe
        wxColour hubCenterColor(30, 30, 32);    // Center horn button

        // 1. Draw Spokes (drawn under the ring and hub for clean overlap)
        dc.SetPen(wxPen(spokeColor, (int)(radius * 0.12), wxPENSTYLE_SOLID));

        // We have 3 spokes: Left (M_PI), Right (0), Down (M_PI/2)
        double spokes[] = { m_angle, m_angle + M_PI, m_angle + M_PI_2 };
        for (int i = 0; i < 3; ++i)
        {
            double a = spokes[i];
            int xStart = cx + (int)(hubRadius * cos(a));
            int yStart = cy + (int)(hubRadius * sin(a));
            int xEnd = cx + (int)((radius - ringThickness / 2) * cos(a));
            int yEnd = cy + (int)((radius - ringThickness / 2) * sin(a));
            dc.DrawLine(xStart, yStart, xEnd, yEnd);
        }

        // 2. Draw Outer Ring
        // Since wxDC::DrawCircle takes the center and radius, and uses the current pen,
        // we can set a thick pen and draw a circle of radius - ringThickness/2.
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(wheelColor, (int)ringThickness, wxPENSTYLE_SOLID));
        dc.DrawCircle(cx, cy, (int)(radius - ringThickness / 2));

        // 3. Draw Center Hub (outer silver ring and inner horn button)
        dc.SetPen(wxPen(spokeColor, 2, wxPENSTYLE_SOLID));
        dc.SetBrush(wxBrush(spokeColor));
        dc.DrawCircle(cx, cy, (int)hubRadius);

        dc.SetPen(wxPen(hubCenterColor, 1, wxPENSTYLE_SOLID));
        dc.SetBrush(wxBrush(hubCenterColor));
        dc.DrawCircle(cx, cy, (int)(hubRadius * 0.75));

        // 4. Draw Red Racing Stripe at the top of the wheel (angle: m_angle - M_PI_2)
        double stripeAngle = m_angle - M_PI_2;
        dc.SetPen(wxPen(stripeColor, (int)(ringThickness * 1.2), wxPENSTYLE_SOLID));

        // Draw the stripe as a small line segment on the ring
        int xStripeStart = cx + (int)((radius - ringThickness) * cos(stripeAngle));
        int yStripeStart = cy + (int)((radius - ringThickness) * sin(stripeAngle));
        int xStripeEnd = cx + (int)(radius * cos(stripeAngle));
        int yStripeEnd = cy + (int)(radius * sin(stripeAngle));
        dc.DrawLine(xStripeStart, yStripeStart, xStripeEnd, yStripeEnd);

        // 5. Add a modern central decorative dot or "horn" icon
        dc.SetPen(wxPen(wxColour(100, 100, 105), 1, wxPENSTYLE_SOLID));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawCircle(cx, cy, (int)(hubRadius * 0.35));
    }
};

#endif // STEERINGWHEELCTRL_H