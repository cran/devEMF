/* $Id$
    --------------------------------------------------------------------------
    Add-on package to R to produce EMF graphics output (for import as
    a high-quality vector graphic into Microsoft Office or OpenOffice).


    Copyright (C) 2011,2015 Philip Johnson

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.


    Note this header file is C++ (R policy requires that all headers
    end with .h).

    This header contains platform-specific code for accessing font
    metric information directly from the windowing system.
    --------------------------------------------------------------------------
*/

#ifndef __APPLE__
#ifndef WIN32
//i.e., hopefully linux/unix!

#ifdef HAVE_XFT
#include <X11/Xft/Xft.h>
#endif
#ifdef HAVE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#endif
struct SPathOutlineFuncs : public FT_Outline_Funcs {
    SPathOutlineFuncs(void) {
        move_to = (FT_Outline_MoveToFunc) MoveTo;
        line_to = (FT_Outline_LineToFunc) LineTo;
        conic_to = (FT_Outline_ConicToFunc) ConicTo;
        cubic_to = (FT_Outline_CubicToFunc) CubicTo;
        shift = 0;
        delta = 0;
    }
    static int MoveTo(const FT_Vector* to, EMFPLUS::SPath *path) {
        //note coordinates are in 26.6 fixed-point format
        path->StartNewPoly((double)to->x/64, (double)to->y/64);
        //Rprintf("start: %d, %d\n", to->x, to->y);
        return 0;
    }
    static int LineTo(const FT_Vector* to, EMFPLUS::SPath *path) {
        path->AddLineTo((double)to->x/64, (double)to->y/64);
        //Rprintf("line: %d, %d\n", to->x, to->y);
        return 0;
    }
    static int ConicTo(const FT_Vector* control, const FT_Vector* to,
                       EMFPLUS::SPath *path) {
        path->AddQuadBezierTo((double)control->x/64,
                              (double)control->y/64,
                              (double)to->x/64,
                              (double)to->y/64);
        //Rprintf("quad: (%d,%d), %d, %d\n", control->x, control->y, to->x, to->y);
        return 0;
    }
    static int CubicTo(const FT_Vector* control1, const FT_Vector* control2,
                       const FT_Vector* to, EMFPLUS::SPath *path) {
        path->AddCubicBezierTo((double)control1->x/64,
                               (double)control1->y/64,
                               (double)control2->x/64,
                               (double)control2->y/64,
                               (double)to->x/64, (double)to->y/64);
        //Rprintf("cubic: (%d,%d), (%d,%d) %d, %d\n", control1->x, control1->y, control2->x, control2->y, to->x, to->y);
        return 0;
    }
};

void SSysFontInfo::AppendGlyphPath(unsigned int c, EMFPLUS::SPath &path) const {
#ifdef HAVE_FREETYPE
#ifdef HAVE_XFT
    if (!m_FontInfo) {
        Rf_error("Font (%s) not found by Xft so can't embed fonts!",
                 m_Spec.m_Family.c_str());
    }
    FT_Face face = XftLockFace(m_FontInfo);
    FT_Matrix transform; //note flipping around y for emf's coord system
    transform.xx = 65536; transform.xy = 0;
    transform.yx = 0; transform.yy = -65536;
    FT_Set_Transform(face, &transform, NULL);
    FT_Set_Pixel_Sizes(face, m_Spec.m_Size, 0);
    int err = FT_Load_Char(face, c, FT_LOAD_NO_BITMAP);
    if (err != 0) {
        Rf_warning("Could not find font outline for embedding '%c'",c);
    }
    if (face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
        SPathOutlineFuncs myFuncs;
        FT_Outline_Decompose(&face->glyph->outline,
                             &myFuncs, &path);
    }
    XftUnlockFace(m_FontInfo);
    //Rprintf("totalpts: %d\n", path.m_TotalPts);
#endif
#endif
}

int SSysFontInfo::GetAdvance(unsigned long prevC, unsigned long nextC) const {
#ifdef HAVE_FREETYPE
#ifdef HAVE_XFT
    if (!m_FontInfo) {
        Rf_error("Font (%s) not found by Xft so can't embed fonts!",
                 m_Spec.m_Family.c_str());
    }
    FT_Face face = XftLockFace(m_FontInfo);
    FT_Set_Pixel_Sizes(face, m_Spec.m_Size, 0);
    unsigned int prevI = FT_Get_Char_Index(face, prevC);
    unsigned int nextI = FT_Get_Char_Index(face, nextC);
    FT_Vector kerning;
    FT_Get_Kerning(face, prevI, nextI, FT_KERNING_DEFAULT, &kerning);
    XftUnlockFace(m_FontInfo);
    FT_Load_Glyph(face, prevI, FT_LOAD_NO_BITMAP);
    return (face->glyph->advance.x>>6) + (kerning.x>>6);
#endif
#endif                                       
}

#endif /* end not windows */
#endif /* end not mac */

/***************************************************************************/

#ifdef WIN32
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef FALSE //sigh.. windows #defs are a pain
#undef TRUE  //sigh.. windows #defs are a pain


#endif /* end WIN32 */

/***************************************************************************/

#ifdef __APPLE__
#include <CoreText/CTFont.h>
#include <CoreText/CTFontDescriptor.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>
#undef TRUE //sigh..it's not just windows
#undef FALSE

#endif /* end __APPLE__ */


        /*
          const FT_Outline &o = ((FT_OutlineGlyph) glyph)->outline;
          unsigned int pointI = 0;
          int pointA=-1, pointB=-1, ctlA=-1, ctlB=-1;
          for (unsigned int contourI = 0;  contourI < o.n_contours;
          ++contourI) {
          path.StartNewPoly();
                
          for (; pointI < o.contours[contourI];  ++pointI) {
          if (o.tags[pointI] & 0x1) {//normal point
          if (pointA == -1) {
          pointA = pointI;
          } else {
          pointB = pointI;
          ProcessPoints(pointA, ctlA, ctlB, pointB)
          path.AddLineSegment(o.points[pointA].x,
          o.points[pointA].y,
          o.points[pointB].x,
          o.points[pointB].y);
          ctlA = ctlB = -1;
          pointA = pointB;
          pointB = -1;
          }
          prev = eNormal;
          } else if (o.tags[pointI] & 0x2) {//3rd order Bezier pt
          if (prev == eCtl) {
          ctlB = pointI;
          } else {
          ctlA = pointI;
          }
          prev = eCtl;
          } else {//2nd order Bezier pt
          if (prev == eCtl) {
          pointB = virt; //virtual "on" point
          //process prev curve here
          pointA = pointB;
          }
          ctlA = pointI;
          prev = eCtl;
          }
          o.points[pointI]; //type: FT_Vector is struct of ulong x, y
          }
          }
          //process closing point here.
          if (o.tags[0] & 0x2) { //3rd order Bezier
          if (prev == eCtl) {
          ctlB = pointI;
          } else {
          ctlA = pointI;
          }
          } else if (o.tags[0] & 0x1 == 0) {
          if ()
          }
        */
