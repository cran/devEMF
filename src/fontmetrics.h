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

#ifdef HAVE_ZLIB
#include <zlib.h>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#endif
#ifdef HAVE_XFT
#include <X11/Xft/Xft.h>
#endif

struct SSysFontInfo {
#ifdef HAVE_ZLIB
    static std::map<std::string, std::vector<std::string> > afmPathDB;
    static std::string packagePath;

    struct SCharMetric {
        int code;
        int w;
        std::string name;
        int llx, lly, urx, ury; //bounding box extents
        double width;
        double ascent;
        double descent;
        SCharMetric(void) {
            width = ascent = descent = 0;
        }
    };
    typedef std::map<unsigned int,SCharMetric> TMetrics;
    TMetrics m_AFMCharMetrics;
    SCharMetric m_AFMFontBBox;
#endif
#ifdef HAVE_XFT
    static Display *s_XDisplay; //global connection to X server
    XftFont *m_FontInfo;
#endif
    
    SSysFontInfo(const std::string &family, int size, int face) {
        if (face < 1  ||  face > 4) {
            Rf_error("Invalid font face requested");
        }
    
#ifdef HAVE_ZLIB
        if (afmPathDB.size() == 0) {
            afmPathDB["Courier"].push_back("Courier-ucs.afm");
            afmPathDB["Courier"].push_back("Courier-Bold-ucs.afm");
            afmPathDB["Courier"].push_back("Courier-Oblique-ucs.afm");
            afmPathDB["Courier"].push_back("Courier-BoldOblique-ucs.afm");
            afmPathDB["Helvetica"].push_back("Helvetica-ucs.afm");
            afmPathDB["Helvetica"].push_back("Helvetica-Bold-ucs.afm");
            afmPathDB["Helvetica"].push_back("Helvetica-Oblique-ucs.afm");
            afmPathDB["Helvetica"].push_back("Helvetica-BoldOblique-ucs.afm");
            afmPathDB["sans"] = afmPathDB["Helvetica"];
            afmPathDB["Times"].push_back("Times-Roman-ucs.afm");
            afmPathDB["Times"].push_back("Times-Bold-ucs.afm");
            afmPathDB["Times"].push_back("Times-Italic-ucs.afm");
            afmPathDB["Times"].push_back("Times-BoldItalic-ucs.afm");
            afmPathDB["serif"] = afmPathDB["times"];
            afmPathDB["ZapfDingbats"].push_back("ZapfDingbats-ucs.afm");
            afmPathDB["ZapfDingbats"].push_back("ZapfDingbats-ucs.afm");
            afmPathDB["ZapfDingbats"].push_back("ZapfDingbats-ucs.afm");
            afmPathDB["ZapfDingbats"].push_back("ZapfDingbats-ucs.afm");
            afmPathDB["Symbol"].push_back("Symbol-ucs.afm");
            afmPathDB["Symbol"].push_back("Symbol-ucs.afm");
            afmPathDB["Symbol"].push_back("Symbol-ucs.afm");
            afmPathDB["Symbol"].push_back("Symbol-ucs.afm");

            //find full path to package using R "findPackage" function
            SEXP findPackage, call;
            PROTECT(findPackage = Rf_findFun(Rf_install("find.package"),
                                             R_GlobalEnv));
            PROTECT(call = Rf_lang2(findPackage, Rf_ScalarString
                                    (Rf_mkChar("devEMF"))));
            SEXP res = Rf_eval(call, R_GlobalEnv);
            UNPROTECT(2);
            if (!Rf_isVector(res)  ||  !Rf_isString(res)  ||
                Rf_length(res) != 1) {
                Rf_error("find.package failed to find devEMF install location"
                         " (or uniquely identify location)");
            }
            packagePath = CHAR(STRING_ELT(res, 0));
        }
#endif
#ifdef HAVE_XFT
        m_FontInfo = NULL;
        if (!s_XDisplay) {
            s_XDisplay = XOpenDisplay(NULL);
            if (!s_XDisplay) {
#ifndef HAVE_ZLIB
                Rf_error("Can't open connection to X server to read font "
                         "metric information (and devEMF was not compiled "
                         "with zlib support to allow pulling metrics from "
                         "file).");
#endif
            }
        }
#endif

#ifdef HAVE_XFT
        if (s_XDisplay) {
            m_FontInfo = XftFontOpen
                (s_XDisplay, XDefaultScreen(s_XDisplay),
                 XFT_FAMILY, XftTypeString, family.c_str(),
                 XFT_PIXEL_SIZE, XftTypeInteger, size,
                 XFT_SLANT, XftTypeInteger, (face == 3  || face == 4 ?
                                             XFT_SLANT_ITALIC :
                                             XFT_SLANT_ROMAN),
                 XFT_WEIGHT, XftTypeInteger, (face == 2  ||  face == 4 ?
                                              XFT_WEIGHT_BOLD :
                                              XFT_WEIGHT_MEDIUM),
                 NULL);
            if (m_FontInfo) {
                return;
            }
        }
#endif
#ifdef HAVE_ZLIB
        if (afmPathDB.find(family) == afmPathDB.end()  ||
            afmPathDB[family].size() < (unsigned int) face) {
            Rf_warning("Font metric information not found for family '%s'; "
                       "using 'Helvetica' instead", family.c_str());
            //last-ditch substitute with "Helvetica"
            LoadAFM((packagePath+"/afm/"+afmPathDB["Helvetica"][face-1]+".gz").
                    c_str(), size, true);
        } else {
            LoadAFM((packagePath+"/afm/"+afmPathDB[family][face-1]+".gz").
                    c_str(), size, true);
        }
        //populate extra characters
        if (family != "Symbol") {
            LoadAFM((packagePath+"/afm/"+afmPathDB["Symbol"][face-1]+".gz").
                    c_str(), size, false);
        }
        if (family != "ZapfDingbats") {
            LoadAFM((packagePath+"/afm/"+afmPathDB["ZapfDingbats"][face-1]+
                     ".gz").c_str(), size, false);
        }
#endif
    }
#ifdef HAVE_XFT
    ~SSysFontInfo() {
        if (m_FontInfo) {
            XftFontClose(s_XDisplay, m_FontInfo);
        }
    }
#endif

#ifdef HAVE_ZLIB
    void LoadAFM(const std::string &filename, int size, bool loadFontBBox) {
        const unsigned int buffsize = 512;
        char buff[buffsize];
        gzFile afm = gzopen(filename.c_str(), "rb");
        while (gzgets(afm, buff, buffsize)) {
            std::stringstream iss(buff);
            std::string key;
            iss >> key;
            if (key == "FontBBox"  &&  loadFontBBox) {
                iss >> m_AFMFontBBox.llx >> m_AFMFontBBox.lly
                    >> m_AFMFontBBox.urx >> m_AFMFontBBox.ury;
                m_AFMFontBBox.ascent = m_AFMFontBBox.ury * 0.001 * size;
                m_AFMFontBBox.descent = m_AFMFontBBox.lly * 0.001 * size;
                m_AFMFontBBox.width = (m_AFMFontBBox.urx-m_AFMFontBBox.llx)
                    * 0.001 * size;
            } else if (key == "C") {
                SCharMetric cMetric;
                iss >> std::hex >> cMetric.code >> std::dec >> key;
                while (iss.good()) {
                    if (key == "WX") {
                        iss >> cMetric.w;
                        cMetric.width = cMetric.w * 0.001 * size;
                    } else if (key == "N") {
                        iss >> cMetric.name;
                    } else if (key == "B") {
                        iss >> cMetric.llx >> cMetric.lly
                            >> cMetric.urx >> cMetric.ury;
                        cMetric.ascent = cMetric.ury * 0.001 * size;
                        cMetric.descent = cMetric.lly * 0.001 * size;
                    }
                    iss >> key;
                }
                if (m_AFMCharMetrics.find(cMetric.code) ==
                    m_AFMCharMetrics.end()) {
                    m_AFMCharMetrics[cMetric.code] = cMetric;
                }
            }
        }
        gzclose(afm);
    }
#endif

    bool HasChar(short unsigned int c) const {
#ifdef HAVE_XFT
        if (m_FontInfo) {
            return XftCharExists(s_XDisplay, m_FontInfo, c);
        }
#endif
#ifdef HAVE_ZLIB
        return m_AFMCharMetrics.find(c) != m_AFMCharMetrics.end();
#endif
    }
    void GetMetrics(short unsigned int c, 
                    double &ascent, double &descent, double &width) const {
#ifdef HAVE_XFT
        if (m_FontInfo) {
            XGlyphInfo extents;
            XftTextExtents16(s_XDisplay, m_FontInfo, &c, 1, &extents);
            // See below URL for interpreting XFT extents
            // http://ns.keithp.com/pipermail/fontconfig/2003-June/000492.html
            ascent = extents.y;
            descent = extents.height-extents.y;
            width = extents.xOff;
            return;
        }
#endif
#ifdef HAVE_ZLIB
        TMetrics::const_iterator m= m_AFMCharMetrics.find(c);
        if (m == m_AFMCharMetrics.end()) {
            ascent = 0;
            descent = 0;
            width = 0;
        } else {
            ascent = m->second.ascent;
            descent = m->second.descent;
            width = m->second.width;
        }
#endif
    }
    double GetStrWidth(const char *str) const {
#ifdef HAVE_XFT
        if (m_FontInfo) {
            XGlyphInfo extents;
            XftTextExtentsUtf8(s_XDisplay, m_FontInfo, (unsigned char*) str,
                               strlen(str), &extents);
            return extents.xOff;
        }
#endif
#ifdef HAVE_ZLIB
        void *cd = Riconv_open("UCS-2", "UTF-8");
        if (cd == (void*)(-1)) {
            Rf_error("EMF device failed to convert UTF-8 to UCS-2");
            return 0;
        }
        size_t inLeft = strlen(str);
        const char *in = str;
        size_t outLeft = inLeft*2;//overestimate
        char *ucs2Str = new char[outLeft];
        char *out = ucs2Str;
        size_t res = Riconv(cd, &in, &inLeft, &out, &outLeft);
        if (res != 0) {
            Riconv_close(cd);
            Rf_error("Text string not valid UTF-8");
            delete[] ucs2Str;
            return 0;
        }
        Riconv_close(cd);

        double w = 0;
        unsigned int len = strlen(str)*2 - outLeft;
        for (unsigned int i = 0;  i < len;  i += 2) {
            int c = ((unsigned char) ucs2Str[i]) +
                (((unsigned char) ucs2Str[i+1]) << 8);
            TMetrics::const_iterator m= m_AFMCharMetrics.find(c);
            if (m != m_AFMCharMetrics.end()) {
                w += m->second.width;
            }
        }
        delete[] ucs2Str;
        return w;
#endif
    }
    void GetFontBBox(double &ascent, double &descent, double &width) {
        ascent = descent = width = 0;
#ifdef HAVE_XFT
        if (m_FontInfo) {
            ascent = m_FontInfo->ascent;
            descent = m_FontInfo->descent;
            width = m_FontInfo->max_advance_width;
            return;
        }
#endif
#ifdef HAVE_ZLIB
        ascent = m_AFMFontBBox.ascent;
        descent = m_AFMFontBBox.descent;
        width = m_AFMFontBBox.width;
#endif        
    }
};
#ifdef HAVE_XFT
Display* SSysFontInfo::s_XDisplay = NULL; //global connection to X server
#endif
#ifdef HAVE_ZLIB
std::map<std::string, std::vector<std::string> > SSysFontInfo::afmPathDB;
std::string SSysFontInfo::packagePath;
#endif

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

struct SSysFontInfo {
    HDC m_DC;
    HFONT m_FontHandle;

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
        m_FontHandle = CreateFontIndirect(&lf);
        SelectObject(m_DC, m_FontHandle);
    }
    ~SSysFontInfo(void) {
        DeleteObject(m_FontHandle);
        ReleaseDC(0, m_DC);
    }
    bool HasChar(short unsigned int c) const {
        GLYPHMETRICS metrics;
        const MAT2 matrix = {{0,1}, {0,0}, {0,0}, {0,1}};
        return (GetGlyphOutlineW(m_DC, c, GGO_METRICS, &metrics,
                                 0, NULL, &matrix) != GDI_ERROR);
    }
    void GetMetrics(short unsigned int c, 
                    double &ascent, double &descent, double &width) const {
        GLYPHMETRICS metrics;
        const MAT2 matrix = {{0,1}, {0,0}, {0,0}, {0,1}};
        if (GetGlyphOutlineW(m_DC, c, GGO_METRICS, &metrics, 0, NULL,&matrix) ==
            GDI_ERROR) {
            ascent = 0;
            descent = 0;
            width = 0;
        } else {
            ascent = metrics.gmptGlyphOrigin.y;
            descent = ((int)metrics.gmBlackBoxY <= metrics.gmptGlyphOrigin.y) ?
                0 : metrics.gmBlackBoxY - metrics.gmptGlyphOrigin.y;
            width = metrics.gmCellIncX;
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
    void GetFontBBox(double &ascent, double &descent, double &width) {
        TEXTMETRIC metrics;
        GetTextMetrics(m_DC, &metrics);
        ascent = metrics.tmAscent;
        descent = metrics.tmDescent;
        width = metrics.tmAveCharWidth;
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
                    double &ascent, double &descent, double &width) const {
        CGGlyph glyph;
        UniChar ch = c;
        CTFontGetGlyphsForCharacters (m_FontInfo, &ch, &glyph, 1);
        CGRect extents;
        extents = CTFontGetBoundingRectsForGlyphs(m_FontInfo,
                                                  kCTFontOrientationDefault,
                                                  &glyph, NULL, 1);
        ascent = extents.origin.y+extents.size.height;
        descent = (extents.origin.y > 0) ? 0 : -extents.origin.y;
        width = CTFontGetAdvancesForGlyphs(m_FontInfo,
                                           kCTFontOrientationDefault,
                                           &glyph, NULL, 1);
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
        double w = CTFontGetAdvancesForGlyphs(m_FontInfo,
                                              kCTFontOrientationDefault,
                                              glyphs, NULL, len);
        delete[] glyphs;
        delete[] ch;
        CFRelease(cfStr);
        return w;
    }
    void GetFontBBox(double &ascent, double &descent, double &width) {
        ascent = CTFontGetAscent(m_FontInfo);
        descent = CTFontGetDescent(m_FontInfo);
        width = 0;//todo
    }
};

#endif /* end __APPLE__ */
