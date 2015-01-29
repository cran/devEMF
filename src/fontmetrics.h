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
//i.e., hopefully X11

#include <X11/Xft/Xft.h>

struct SSysFontInfo {
    static Display *s_XDisplay; //global connection to X server
    XftFont *m_FontInfo;

    SSysFontInfo(const char *family, int size, int face) {
        if (!s_XDisplay) {
            s_XDisplay = XOpenDisplay(NULL);
            if (!s_XDisplay) {
                Rf_error("Can't open connection to X server to read font "
                         "metric information.");
            }
        }
        m_FontInfo = XftFontOpen
            (s_XDisplay, XDefaultScreen(s_XDisplay),
             XFT_FAMILY, XftTypeString, family,
             XFT_PIXEL_SIZE, XftTypeInteger, size,
             XFT_SLANT, XftTypeInteger, (face == 3  || face == 4 ?
                                         XFT_SLANT_ITALIC :
                                         XFT_SLANT_ROMAN),
             XFT_WEIGHT, XftTypeInteger, (face == 2  ||  face == 4 ?
                                          XFT_WEIGHT_BOLD :
                                          XFT_WEIGHT_MEDIUM),
             NULL);
    }
    ~SSysFontInfo() {
        XftFontClose(s_XDisplay, m_FontInfo);
    }

    bool HasChar(short unsigned int c) const {
        return XftCharExists(s_XDisplay, m_FontInfo, c);
    }
    void GetMetrics(short unsigned int c, 
                    double *ascent, double *descent, double *width) const {
        XGlyphInfo extents;
        XftTextExtents16(s_XDisplay, m_FontInfo, &c, 1, &extents);
        // see http://ns.keithp.com/pipermail/fontconfig/2003-June/000492.html
        *ascent = extents.y;
        *descent = extents.height-extents.y;
        *width = extents.xOff;
    }
    double GetStrWidth(const char *str) const {
        XGlyphInfo extents;
        XftTextExtentsUtf8(s_XDisplay, m_FontInfo, (unsigned char*) str,
                           strlen(str), &extents);
        return extents.xOff;
    }
};
Display* SSysFontInfo::s_XDisplay = NULL; //global connection to X server
#endif /* end X11 */
#endif

/***************************************************************************/

#ifdef WIN32
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef FALSE //sigh.. windows #defs are a pain
#undef TRUE  //sigh.. windows #defs are a pain

struct SSysFontInfo {
    HDC m_DC;

    SSysFontInfo(const char *family, int size, int face) {
        m_DC = GetDC(0);
        LOGFONT lf;
        lf.lfHeight = -size;//(-) matches against *character* height
        lf.lfWidth = 0;
        lf.lfEscapement = 0;
        lf.lfOrientation = 0;
        lf.lfWeight = (face == 2  ||  face == 4) ?
            FW_BOLD : FW_NORMAL;
        lf.lfItalic = (face == 3  ||  face == 4) ? 0x01 : 0x00;
        lf.lfUnderline = 0x00;
        lf.lfStrikeOut = 0x00;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfOutPrecision = OUT_STROKE_PRECIS;
        lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        lf.lfQuality = DEFAULT_QUALITY;
        lf.lfPitchAndFamily = FF_DONTCARE + DEFAULT_PITCH;
        MultiByteToWideChar(CP_UTF8, 0, family, -1, lf.lfFaceName, LF_FACESIZE);
        HFONT fontHandle = CreateFontIndirect(&lf);
        SelectObject(m_DC, fontHandle);
    }
    bool HasChar(short unsigned int c) const {
        GLYPHMETRICS metrics;
        const MAT2 matrix = {{0,1}, {0,0}, {0,0}, {0,1}};
        return (GetGlyphOutlineW(m_DC, c, GGO_METRICS, &metrics,
                                 0, NULL, &matrix) != GDI_ERROR);
    }
    void GetMetrics(short unsigned int c, 
                    double *ascent, double *descent, double *width) const {
        GLYPHMETRICS metrics;
        const MAT2 matrix = {{0,1}, {0,0}, {0,0}, {0,1}};
        if (GetGlyphOutlineW(m_DC, c, GGO_METRICS, &metrics, 0, NULL,&matrix) ==
            GDI_ERROR) {
            *ascent = 0;
            *descent = 0;
            *width = 0;
        } else {
            *ascent = metrics.gmptGlyphOrigin.y;
            *descent = ((int)metrics.gmBlackBoxY <= metrics.gmptGlyphOrigin.y) ?
                0 : metrics.gmBlackBoxY - metrics.gmptGlyphOrigin.y;
            *width = metrics.gmCellIncX;
        }
    }
    double GetStrWidth(const char *str) const {
        SIZE size;
        int nChar =
            MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
        WCHAR *wStr = new WCHAR[nChar];//nChar includes terminating null!
        MultiByteToWideChar(CP_UTF8, 0, str, -1, wStr, nChar);
        GetTextExtentPoint32W(m_DC, wStr, nChar-1, &size);
        delete[] wStr;
        return size.cx;
    }
};

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
struct SSysFontInfo {
    CTFontRef m_FontInfo;

    SSysFontInfo(const char *family, int size, int face) {
        CFMutableDictionaryRef attr =
            CFDictionaryCreateMutable(NULL, 1, &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks);
        CFStringRef vFam =
            CFStringCreateWithCString(NULL, family, kCFStringEncodingUTF8);
        CFDictionaryAddValue(attr, kCTFontFamilyNameAttribute, vFam);
        float fSize = size;
        CFNumberRef vSize = CFNumberCreate(NULL, kCFNumberFloatType, &fSize);
        CFDictionaryAddValue(attr, kCTFontSizeAttribute, vSize);
        CFMutableDictionaryRef vTrait =
            CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
        float fWeight = (face == 2  ||  face == 4 ? 1 : 0);
        CFNumberRef vWeight =CFNumberCreate(NULL, kCFNumberFloatType, &fWeight);
        float fSlant = (face == 3  ||  face == 4 ? 1 : 0);
        CFNumberRef vSlant = CFNumberCreate(NULL, kCFNumberFloatType, &fSlant);
        CFDictionaryAddValue(vTrait, kCTFontWeightTrait, vWeight);
        CFDictionaryAddValue(vTrait, kCTFontSlantTrait, vSlant);
        CFDictionaryAddValue(attr, kCTFontTraitsAttribute, vTrait);
        CTFontDescriptorRef descriptor =
            CTFontDescriptorCreateWithAttributes(attr);
        m_FontInfo = CTFontCreateWithFontDescriptor (descriptor, size, NULL);
        CFRelease(descriptor);
        CFRelease(attr);
        CFRelease(vSlant);
        CFRelease(vWeight);
        CFRelease(vTrait);
        CFRelease(vSize);
        CFRelease(vFam);
    }
    ~SSysFontInfo() {CFRelease(m_FontInfo);}
    bool HasChar(short unsigned int c) const {
        CGGlyph glyph;
        UniChar ch = c;
        return CTFontGetGlyphsForCharacters (m_FontInfo, &ch, &glyph, 1);
    }
    void GetMetrics(short unsigned int c, 
                    double *ascent, double *descent, double *width) const {
        CGGlyph glyph;
        UniChar ch = c;
        CTFontGetGlyphsForCharacters (m_FontInfo, &ch, &glyph, 1);
        CGRect extents;
        extents = CTFontGetBoundingRectsForGlyphs(m_FontInfo, 0, &glyph,
                                                  NULL, 1);
        *ascent = extents.origin.y+extents.size.height;
        *descent = (extents.origin.y > 0) ? 0 : -extents.origin.y;
        *width = CTFontGetAdvancesForGlyphs(m_FontInfo, 0, &glyph, NULL, 1);
    }
    double GetStrWidth(const char *str) const {
        CFStringRef cfStr =
            CFStringCreateWithCString(NULL, str, kCFStringEncodingUTF8);
        int len = CFStringGetLength(cfStr);
        CGGlyph* ch = new UniChar[len];
        for (int i = 0;  i < len;  ++i) {
            ch[i] = CFStringGetCharacterAtIndex(cfStr, i);
        }
        CGGlyph* glyphs = new CGGlyph[len];
        CTFontGetGlyphsForCharacters (m_FontInfo, ch, glyphs, len);
        double w = CTFontGetAdvancesForGlyphs(m_FontInfo, 0, glyphs, NULL,
                                              len);
        delete[] glyphs;
        delete[] ch;
        CFRelease(cfStr);
        return w;
    }
};

#endif /* end __APPLE__ */
