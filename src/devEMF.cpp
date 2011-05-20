/* $Id: devEMF.cpp 175 2011-05-20 15:49:32Z pjohnson $
    --------------------------------------------------------------------------
    Add-on package to R to produce EMF graphics output (for import as
    a high-quality vector graphic into Microsoft Office or OpenOffice).


    Copyright (C) 2011 Philip Johnson

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


    If building library outside of R package (i.e. for debugging):
        R CMD SHLIB devEMF.cpp

    --------------------------------------------------------------------------
*/

#include <Rconfig.h>

#define STRICT_R_HEADERS
#define R_NO_REMAP
#include <R.h>
#include <Rinternals.h>

#include <R_ext/GraphicsEngine.h>
#include <R_ext/Rdynload.h>
#include <R_ext/Riconv.h>

#include <fstream>
#include <set>

#include "emf.h"      //declares EMF objects
#include "ps_fonts.h" //font metric code from devPS.c included here

using namespace std;
using namespace EMF;

class CDevEMF {
public:
    CDevEMF(bool userLty, const char *defaultFontFamily) : m_debug(false) {
        m_UseUserLty = userLty;
        m_PostscriptFonts = NULL;
	addEncoding("ISOLatin1.enc", FALSE);
        AddFont(defaultFontFamily);
        m_DefaultFontFamily = defaultFontFamily;
        m_PageNum = 0;
        m_NumRecords = 0;
        m_LastHandle = m_CurrPen = m_CurrFont = m_CurrBrush = 0;
        m_CurrHadj = m_CurrTextCol = -1;
    }
    type1fontfamily AddFont(const char *family) {
        int gotFont;
        type1fontfamily
            font = addFont(family, FALSE, loadedEncodings);
        m_PostscriptFonts = addDeviceFont(font, m_PostscriptFonts, &gotFont);
        return font;
    }

    // Member-function R callbacks (see below class definition for
    // extern "C" versions
    bool Open(const char* filename, int width, int height);
    void Close(void);
    void NewPage(const pGEcontext gc);
    void MetricInfo(int c, const pGEcontext gc, double* ascent,
                    double* descent, double* width);
    double StrWidth(const char *str, const pGEcontext gc);
    void Clip(double x0, double x1, double y0, double y1);
    void Circle(double x, double y, double r, const pGEcontext gc);
    void Line(double x1, double y1, double x2, double y2, const pGEcontext gc);
    void Polyline(int n, double *x, double *y, const pGEcontext gc);
    void TextUTF8(double x, double y, const char *str, double rot,
                  double hadj, const pGEcontext gc);

    void Rect(double x0, double y0, double x1, double y1, const pGEcontext gc);
    void Polygon(int n, double *x, double *y, const pGEcontext gc);

    // global helper functions
    static int x_Inches2Dev(double inches) { return 2540*inches;}
    static double x_EffPointsize(const pGEcontext gc) {
        return floor(gc->cex * gc->ps + 0.5);
    }

private:
    static string iConvUTF8toUTF16LE(const string& s) {
        void *cd = Riconv_open("UTF-16LE", "UTF-8");
        if (cd == (void*)(-1)) {
            Rf_error("EMF device failed to convert UTF-8 to UTF-16LE");
            return "";
        }
        size_t inLeft = s.length();
        size_t outLeft = s.length()*2;
        char *utf16 = new char[outLeft];//overestimate
        const char *in = s.c_str();
        char *out = utf16;
        size_t res = Riconv(cd, &in, &inLeft, &out, &outLeft);
        if (res != 0) {
            delete[] utf16;
            Rf_error("Text string not valid UTF-8.");
            return "";
        }
        string ret(utf16, s.length()*2 - outLeft);
        delete[] utf16;
        Riconv_close(cd);
        return ret;
    }
    void x_TransformY(double* y, int n) {
        for (int i = 0; i < n;  ++i, ++y) *y = m_Height - *y;
    }

    //Helper classes for creating EMF records
    template<class T> 
    void x_WriteRcd(const T &r) {
	string buff; r.Serialize(buff);
	buff.resize(((buff.size() + 3)/4)*4, '\0'); //add padding
        string nSize;
        nSize << EMF::TUInt4(buff.size());
	buff.replace(4,4, nSize);
	m_File.write(buff.data(), buff.size());
	++m_NumRecords;
    }

    struct SPen : EMF::S_EXTCREATEPEN {
        SPen(int lty, int lwd, int col, bool useUserLty) {
            ihPen = 0;
            offBmi = cbBmi = offBits = cbBits = 0;
            elp.penStyle = PS_GEOMETRIC | (R_TRANSPARENT(col) ? PS_NULL : PS_USERSTYLE);
            elp.width = lwd;
            elp.brushStyle = BS_SOLID;
            elp.color.Set(R_RED(col), R_GREEN(col), R_BLUE(col));
            elp.brushHatch = 0;
            elp.numEntries = 0;
            if (R_TRANSPARENT(col)) {
                return;
            }
            if (!useUserLty) {
                elp.penStyle = PS_GEOMETRIC;
                switch(lty) {
                case LTY_SOLID: elp.penStyle |= PS_SOLID; break;
                case LTY_DASHED: elp.penStyle |= PS_DASH; break;
                case LTY_DOTTED: elp.penStyle |= PS_DOT; break;
                case LTY_DOTDASH: elp.penStyle |= PS_DASHDOT; break;
                case LTY_LONGDASH: elp.penStyle |= PS_DASHDOTDOT; break;
                default: elp.penStyle |= PS_SOLID;
                    Rf_warning("Using lty unsupported by EMF device");
                }
            } else {
                for (int tmp = lty;  elp.numEntries < 8  &&  tmp & 15;
                     ++elp.numEntries, tmp >>= 4);
                if (elp.numEntries == 0) {
                    elp.penStyle = PS_SOLID | PS_GEOMETRIC;
                } else {
                    styleEntries = new TUInt4[elp.numEntries];
                    for(int i = 0;  i < 8  &&  lty & 15;  ++i, lty >>= 4) {
                        styleEntries[i] = x_Inches2Dev((lty & 15)/72.);
                    }
                }
            }
	}
    };
    class CPens : public set<SPen*, SPen::Less> {
    public: ~CPens(void) {
        for (iterator i = begin();  i != end();  ++i) { delete *i; }
    }
    };
    
    struct SBrush : EMF::SBrush {
	SBrush(int col) {
	    ihBrush = 0; //must be reset later
            lb.brushStyle = (R_TRANSPARENT(col) ? BS_NULL : BS_SOLID);
            lb.color.Set(R_RED(col), R_GREEN(col), R_BLUE(col));
            lb.brushHatch = 0; //unused with BS_SOLID or BS_NULL
	}
    };
    typedef set<SBrush> CBrushes;

    struct SFont : EMF::SFont {
	SFont(int face, int size, int rot, const char* family) {
	    ihFont = 0; //must be reset later
	    lf.height = -size;//(-) matches against *character* height
	    lf.width = 0;
	    lf.escapement = rot*10;
	    lf.orientation = 0;
	    lf.weight = (face == 2  ||  face == 4) ?
                eFontWeight_bold : eFontWeight_normal;
	    lf.italic = (face == 3  ||  face == 4) ? 0x01 : 0x00;
	    lf.underline = 0x00;
	    lf.strikeOut = 0x00;
	    lf.charSet = DEFAULT_CHARSET;
	    lf.outPrecision = OUT_STROKE_PRECIS;
	    lf.clipPrecision = CLIP_DEFAULT_PRECIS;
	    lf.quality = DEFAULT_QUALITY;
	    lf.pitchAndFamily = FF_DONTCARE + DEFAULT_PITCH;
            lf.SetFace(iConvUTF8toUTF16LE(family)); //assume UTF8/ASCII family
	}
    };
    typedef set<SFont> CFonts;

    void x_SetLinetype(int lty, int lwd, int col) {
	if (m_debug) fprintf(stderr, "lty:%i; lwd:%i\n", lty, lwd);
	SPen *newPen = new SPen(lty, x_Inches2Dev(lwd/96.), col,
                                m_UseUserLty);
	CPens::iterator i = m_Pens.find(newPen);
	if (i == m_Pens.end()) {
            if (R_ALPHA(col) > 0  &&  R_ALPHA(col) < 255) {
                Rf_warning("partial transparency is not supported for EMF");
            }
	    newPen->ihPen = ++m_LastHandle;
	    i = m_Pens.insert(newPen).first;
	    x_WriteRcd(*newPen);
	} else {
            delete newPen;
        }
	if ((*i)->ihPen != m_CurrPen) {
	    x_CreateRcdSelectObj((*i)->ihPen);
	    m_CurrPen = (*i)->ihPen;
	}
    }
    void x_SetFill(int col) {
	if (m_debug) fprintf(stderr, "fill:%i\n", col);
	SBrush newBrush(col);
	CBrushes::iterator i = m_Brushes.find(newBrush);
	if (i == m_Brushes.end()) {
            if (R_ALPHA(col) > 0  &&  R_ALPHA(col) < 255) {
                Rf_warning("partial transparency is not supported for EMF");
            }
	    newBrush.ihBrush = ++m_LastHandle;
	    i = m_Brushes.insert(newBrush).first;
	    x_WriteRcd(newBrush);
	}
	if (i->ihBrush != m_CurrBrush) {
	    x_CreateRcdSelectObj(i->ihBrush);
	    m_CurrBrush = i->ihBrush;
	}
    }
    void x_SetFont(int face, double size, int rot, const char *family) {
        if (family[0] == '\0') {
            family = m_DefaultFontFamily.c_str();
        }
	if (m_debug) fprintf(stderr, "set font face:%i; size:%.1f; rot:%i\n", face, size, rot);
	SFont newFont(face, x_Inches2Dev(size/72.), rot, family);
	CFonts::iterator i = m_Fonts.find(newFont);
	if (i == m_Fonts.end()) {
	    newFont.ihFont = ++m_LastHandle;
	    i = m_Fonts.insert(newFont).first;
	    x_WriteRcd(newFont);
	}
	if (i->ihFont != m_CurrFont) {
	    x_CreateRcdSelectObj(i->ihFont);
	    m_CurrFont = i->ihFont;
	}
    }
    void x_SetHAdj(double hadj) {
        if (m_CurrHadj != hadj) {
            EMF::S_SETTEXTALIGN emr;
            if (hadj == 0.) {
                emr.mode = TA_BOTTOM|TA_LEFT;
            } else if (hadj == 1.) {
                emr.mode = TA_BOTTOM|TA_RIGHT;
            } else {
                emr.mode = TA_BOTTOM|TA_CENTER;
            }
            x_WriteRcd(emr);
            m_CurrHadj = hadj;
        }
    }
    void x_SetTextColor(int col) {
        if (m_CurrTextCol != col) {
            EMF::S_SETTEXTCOLOR emr;
            emr.color.Set(R_RED(col), R_GREEN(col), R_BLUE(col));
            x_WriteRcd(emr);
            m_CurrTextCol = col;
        }
    }

    void x_CreateRcdHeader(void);
    void x_CreateRcdSelectObj(unsigned int obj) {
        EMF::S_SELECTOBJECT emr;
	emr.ihObject = obj;
        x_WriteRcd(emr);
    }

private:
    bool m_debug;
    ofstream m_File;
    int m_NumRecords;
    int m_PageNum;
    int m_Width, m_Height;
    bool m_UseUserLty;
    string m_DefaultFontFamily;
    type1fontlist m_PostscriptFonts;

    //states
    double m_CurrHadj;
    int m_CurrTextCol;

    //objects
    int m_LastHandle;
    CPens m_Pens;       unsigned int m_CurrPen;
    CBrushes m_Brushes; unsigned int m_CurrBrush;
    CFonts m_Fonts;     unsigned int m_CurrFont;
};

// R callbacks below (declare extern "C")
namespace EMFcb {
    extern "C" {
        void Activate(pDevDesc) {}
        void Deactivate(pDevDesc) {}
        void Mode(int, pDevDesc) {}
        Rboolean Locator(double*, double*, pDevDesc) {return FALSE;}
        void Raster(unsigned int*, int, int, double, double, double,
                    double, double, Rboolean, const pGEcontext, pDevDesc) {
            Rf_warning("Raster rending not yet implemented for EMF");
        }
        SEXP Cap(pDevDesc) {
            Rf_warning("Raster capture not available for this EMF");
            return R_NilValue;
        }
        void Path(double*, double*, int, int*, Rboolean, const pGEcontext,
                  pDevDesc) {
            Rf_warning("Path rending not yet implemented for EMF.");
        }
        void Close(pDevDesc dd) {
            static_cast<CDevEMF*>(dd->deviceSpecific)->Close();
            delete static_cast<CDevEMF*>(dd->deviceSpecific);
        }
        void NewPage(const pGEcontext gc, pDevDesc dd) {
            static_cast<CDevEMF*>(dd->deviceSpecific)->NewPage(gc);
        }
        void MetricInfo(int c, const pGEcontext gc, double* ascent,
                        double* descent, double* width, pDevDesc dd) {
            static_cast<CDevEMF*>(dd->deviceSpecific)->
                MetricInfo(c, gc, ascent,descent, width);
        }
        double StrWidth(const char *str, const pGEcontext gc, pDevDesc dd) {
            return static_cast<CDevEMF*>(dd->deviceSpecific)->StrWidth(str, gc);
        }
        void Clip(double x0, double x1, double y0, double y1, pDevDesc dd) {
            static_cast<CDevEMF*>(dd->deviceSpecific)->Clip(x0,x1,y0,y1);
        }
        void Circle(double x, double y, double r, const pGEcontext gc,
                    pDevDesc dd) {
            static_cast<CDevEMF*>(dd->deviceSpecific)->Circle(x,y,r,gc);
        }
        void Line(double x1, double y1, double x2, double y2,
                  const pGEcontext gc, pDevDesc dd) {
            static_cast<CDevEMF*>(dd->deviceSpecific)->Line(x1,y1,x2,y2,gc);
        }
        void Polyline(int n, double *x, double *y, 
                      const pGEcontext gc, pDevDesc dd) {
            static_cast<CDevEMF*>(dd->deviceSpecific)->Polyline(n,x,y, gc);
        }
        void TextUTF8(double x, double y, const char *str, double rot,
                      double hadj, const pGEcontext gc, pDevDesc dd) {
            static_cast<CDevEMF*>(dd->deviceSpecific)->
                TextUTF8(x,y,str,rot,hadj,gc);
        }
        void Text(double, double, const char *, double,
                  double, const pGEcontext, pDevDesc) {
            Rf_warning("Non-UTF8 text currently unsupported in devEMF.");
        }
        void Rect(double x0, double y0, double x1, double y1,
                  const pGEcontext gc, pDevDesc dd) {
            static_cast<CDevEMF*>(dd->deviceSpecific)->Rect(x0,y0,x1,y1,gc);
        }
        void Polygon(int n, double *x, double *y, 
                     const pGEcontext gc, pDevDesc dd) {
            static_cast<CDevEMF*>(dd->deviceSpecific)->Polygon(n,x,y,gc);
        }

        void Size(double *left, double *right, double *bottom, double *top,
                  pDevDesc dd) {
            *left = dd->left; *right = dd->right;
            *bottom = dd->bottom; *top = dd->top;
        }
    }
} //end of R callbacks / extern "C"

/* Table copied from R util.c, which got from
http://unicode.org/Public/MAPPINGS/VENDORS/ADOBE/symbol.txt
*/

static int symbol2unicode[224] = {
    0x0020, 0x0021, 0x2200, 0x0023, 0x2203, 0x0025, 0x0026, 0x220D,
    0x0028, 0x0029, 0x2217, 0x002B, 0x002C, 0x2212, 0x002E, 0x002F,
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037,
    0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
    0x2245, 0x0391, 0x0392, 0x03A7, 0x0394, 0x0395, 0x03A6, 0x0393,
    0x0397, 0x0399, 0x03D1, 0x039A, 0x039B, 0x039C, 0x039D, 0x039F,
    0x03A0, 0x0398, 0x03A1, 0x03A3, 0x03A4, 0x03A5, 0x03C2, 0x03A9,
    0x039E, 0x03A8, 0x0396, 0x005B, 0x2234, 0x005D, 0x22A5, 0x005F,
    0xF8E5, 0x03B1, 0x03B2, 0x03C7, 0x03B4, 0x03B5, 0x03C6, 0x03B3,
    0x03B7, 0x03B9, 0x03D5, 0x03BA, 0x03BB, 0x03BC, 0x03BD, 0x03BF,
    0x03C0, 0x03B8, 0x03C1, 0x03C3, 0x03C4, 0x03C5, 0x03D6, 0x03C9,
    0x03BE, 0x03C8, 0x03B6, 0x007B, 0x007C, 0x007D, 0x223C, 0x0020,
    0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
    0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
    0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
    0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
    0x20AC, 0x03D2, 0x2032, 0x2264, 0x2044, 0x221E, 0x0192, 0x2663,
    0x2666, 0x2665, 0x2660, 0x2194, 0x2190, 0x2191, 0x2192, 0x2193,
    0x00B0, 0x00B1, 0x2033, 0x2265, 0x00D7, 0x221D, 0x2202, 0x2022,
    0x00F7, 0x2260, 0x2261, 0x2248, 0x2026, 0xF8E6, 0xF8E7, 0x21B5,
    0x2135, 0x2111, 0x211C, 0x2118, 0x2297, 0x2295, 0x2205, 0x2229,
    0x222A, 0x2283, 0x2287, 0x2284, 0x2282, 0x2286, 0x2208, 0x2209,
    0x2220, 0x2207, 0xF6DA, 0xF6D9, 0xF6DB, 0x220F, 0x221A, 0x22C5,
    0x00AC, 0x2227, 0x2228, 0x21D4, 0x21D0, 0x21D1, 0x21D2, 0x21D3,
    0x25CA, 0x2329, 0xF8E8, 0xF8E9, 0xF8EA, 0x2211, 0xF8EB, 0xF8EC,
    0xF8ED, 0xF8EE, 0xF8EF, 0xF8F0, 0xF8F1, 0xF8F2, 0xF8F3, 0xF8F4,
    0x0020, 0x232A, 0x222B, 0x2320, 0xF8F5, 0x2321, 0xF8F6, 0xF8F7,
    0xF8F8, 0xF8F9, 0xF8FA, 0xF8FB, 0xF8FC, 0xF8FD, 0xF8FE, 0x0020
};


void CDevEMF::MetricInfo(int c, const pGEcontext gc, double* ascent,
                     double* descent, double* width)
{
    if (m_debug) fprintf(stderr, "metricinfo\n");
    //cout << gc->fontfamily << "/" << gc->fontface << " -- " << c << " / " << (char) c;
    if (!m_PostscriptFonts) { //no metrics available
        *ascent = 0;
        *descent = 0;
        *width = 0;
        return;
    }

    int face = gc->fontface;
    if(face < 1 || face > 5) face = 1;
    int fontIndex;//dummy
    type1fontfamily family = findDeviceFont(gc->fontfamily, m_PostscriptFonts,
                                            &fontIndex);
    if (!family) {
        family = AddFont(gc->fontfamily);
        if (!family) { //no metrics available
            *ascent = 0;
            *descent = 0;
            *width = 0;
            return;
        }
    }
    if (face == 5  &&  c < 0) {
        //postscript metric info wants AdobeSymbol encoding, not
        //unicode, so we (awkwardly) switch
        int s;
        for (s=0;  s<224  &&  symbol2unicode[s] != -c;  ++s);
        if (s < 224) {
            c = s + 32;
        }
    }
    PostScriptMetricInfo(c, ascent, descent, width,
                         &(family->fonts[face-1]->metrics),
                         FALSE, family->encoding->convname);

    *ascent = x_Inches2Dev(x_EffPointsize(gc) * *ascent/72.);
    *descent = x_Inches2Dev(x_EffPointsize(gc) * *descent/72.);
    *width = x_Inches2Dev(x_EffPointsize(gc) * *width/72.);
}


double CDevEMF::StrWidth(const char *str, const pGEcontext gc) {
    if (m_debug) fprintf(stderr, "strwidth\n");
    int face = gc->fontface;
    if(face < 1 || face > 5) face = 1;

    //FIXME: kerning
    double a, d, w, width = 0;

    string utf16 = iConvUTF8toUTF16LE(str);
    int nChars = utf16.length()/2;

    unsigned short *c = (unsigned short*) utf16.data();
    for (int i = 0;  i < nChars;  ++i, ++c) {
#ifdef WORDS_BIGENDIAN
        *c = ((*c >> 8) & 0xFF) | ((*c & 0xFF) << 8);
#endif
        MetricInfo(-*c, gc, &a, &d, &w);
        if (a == 0  &&  d == 0  &&  w == 0) { //failure
            return 0;
        }
        width += w;
    }

    //cout << "strwidth: " << width/CDevEMF::x_Inches2Dev(1) << endl;
    return width;
}

	/* Initialize the device */

void CDevEMF::x_CreateRcdHeader(void) {
    {
        EMF::SHeader emr;
        emr.bounds.Set(0,0,m_Width,m_Height);
        emr.frame.Set(0,0,m_Width,m_Height);
        emr.signature = 0x464D4520;
        emr.version = 0x00010000;
        emr.nBytes = 0;   //WILL EDIT WHEN CLOSING
        emr.nRecords = 0; //WILL EDIT WHEN CLOSING
        emr.nHandles = 0; //WILL EDIT WHEN CLOSING
        emr.reserved = 0x0000;
        //Description string must be UTF-16LE
        emr.desc = iConvUTF8toUTF16LE("Created by R using devEMF.");
        emr.nDescription = emr.desc.length()/2;
        emr.offDescription = 0; //set during serialization
        emr.nPalEntries = 0;
        emr.device.Set(m_Width,m_Height);
        emr.millimeters.Set(m_Width/100,m_Height/100);
        emr.cbPixelFormat=0x00000000;
        emr.offPixelFormat=0x00000000;
        emr.bOpenGL=0x00000000;
        emr.micrometers.Set(m_Width*10, m_Height*10);
        x_WriteRcd(emr);
    }

    {//Don't paint text box background
        EMF::S_SETBKMODE emr;
        emr.mode = TRANSPARENT;
        x_WriteRcd(emr);
    }

    {//Logical units == device units
        EMF::S_SETMAPMODE emr;
        emr.mode = MM_TEXT;
        x_WriteRcd(emr);
    }
}


bool CDevEMF::Open(const char* filename, int width, int height)
{
    if (m_debug) fprintf(stderr, "open: %i, %i\n", width, height);
    m_Width = width;
    m_Height = height;
    
    m_File.open(R_ExpandFileName(filename), ios_base::binary);
    if (!m_File) {
	return FALSE;
    }
    x_CreateRcdHeader();
    return TRUE;
}

void CDevEMF::NewPage(const pGEcontext gc) {
    if (++m_PageNum > 1) {
        Rf_warning("Multiple pages not available for EMF device");
    }
    if (R_OPAQUE(gc->fill)) {
	gc->col = R_TRANWHITE; // no line around border
        Rect(0, 0, m_Width, m_Height, gc);
    }
}


void CDevEMF::Clip(double x0, double x1, double y0, double y1)
{
    if (m_debug) fprintf(stderr, "clip\n");
    return;
}


void CDevEMF::Close(void)
{
    if (m_debug) fprintf(stderr, "close\n");
    
    { //EOF record
        EMF::S_EOF emr;
        emr.nPalEntries = 0;
        emr.offPalEntries = 0;
        emr.nSizeLast = sizeof(emr);
        x_WriteRcd(emr);
    }

    { //Edit header record to report number of records, handles & size
        EMF::SHeader emr;
        emr.nBytes = m_File.tellp();
        emr.nRecords = m_NumRecords;
        //not mentioned in spec, but seems to need one extra handle
        emr.nHandles = m_LastHandle+1;
        m_File.seekp(4*12);//offset of nBytes
        string data;
        data << EMF::TUInt4(emr.nBytes) << EMF::TUInt4(emr.nRecords) << EMF::TUInt4(emr.nHandles);
        m_File.write(data.data(), 12);
        m_File.close();
    }
}

	/* Draw To */

void CDevEMF::Line(double x1, double y1, double x2, double y2,
	       const pGEcontext gc)
{
    if (m_debug) fprintf(stderr, "line\n");

    if (x1 != x2  ||  y1 != y2) {
	x_SetLinetype(gc->lty, gc->lwd, gc->col);
        double x[2], y[2];
        x[0] = x1; x[1] = x2;
        y[0] = y1; y[1] = y2;
        Polyline(2, x, y, gc);
    }
}

void CDevEMF::Polyline(int n, double *x, double *y, const pGEcontext gc)
{
    if (m_debug) fprintf(stderr, "polyline\n");
    x_SetLinetype(gc->lty, gc->lwd, gc->col);

    x_TransformY(y, n);//EMF has origin in upper left; R in lower left
    EMF::SPoly polyline(EMR_POLYLINE, n, x, y);
    x_WriteRcd(polyline);
}

void CDevEMF::Rect(double x0, double y0, double x1, double y1, const pGEcontext gc)
{
    if (m_debug) fprintf(stderr, "rect\n");
    x_SetLinetype(gc->lty, gc->lwd, gc->col);
    x_SetFill(gc->fill);

    x_TransformY(&y0, 1);//EMF has origin in upper left; R in lower left
    x_TransformY(&y1, 1);//EMF has origin in upper left; R in lower left
    
    EMF::S_RECTANGLE emr;
    emr.box.Set(lround(x0), lround(y0), lround(x1), lround(y1));
    x_WriteRcd(emr);
}

void CDevEMF::Circle(double x, double y, double r, const pGEcontext gc)
{
    if (m_debug) fprintf(stderr, "circle\n");

    x_SetLinetype(gc->lty, gc->lwd, gc->col);
    x_SetFill(gc->fill);

    x_TransformY(&y, 1);//EMF has origin in upper left; R in lower left
    EMF::S_ELLIPSE emr;
    emr.box.Set(lround(x-r), lround(y-r), lround(x+r), lround(y+r));
    x_WriteRcd(emr);
}

void CDevEMF::Polygon(int n, double *x, double *y, const pGEcontext gc)
{
    if (m_debug) fprintf(stderr, "polygon\n");

    x_SetLinetype(gc->lty, gc->lwd, gc->col);
    x_SetFill(gc->fill);

    x_TransformY(y, n);//EMF has origin in upper left; R in lower left
    EMF::SPoly polygon(EMR_POLYGON, n, x, y);
    x_WriteRcd(polygon);
}

void CDevEMF::TextUTF8(double x, double y, const char *str, double rot,
                   double hadj, 
	       const pGEcontext gc)
{
    if (m_debug) fprintf(stderr, "textUTF8: %s, %x  at %.1f %.1f\n", str, gc->col, x, y);

    x_SetFont(gc->fontface, x_EffPointsize(gc), rot, gc->fontfamily);
    x_SetHAdj(hadj);
    x_SetTextColor(gc->col);

    {//R assumes vertical centering; EMF places at baseline
        double a, d, w, ascent = 0;
        for (const char *c = str;  *c;  ++c) {
            MetricInfo(*c, gc, &a, &d, &w);
            if (a > ascent) {
                ascent = a;
            }
        }
        x += (ascent/2.) * sin(rot*M_PI/180);
        y -= (ascent/2.) * cos(rot*M_PI/180);
    }
    x_TransformY(&y, 1);//EMF has origin in upper left; R in lower left
    EMF::S_EXTTEXTOUTW emr;
    emr.bounds.Set(0,0,0,0);//EMF spec says to ignore
    emr.graphicsMode = GM_COMPATIBLE;
    emr.exScale = 1;
    emr.eyScale = 1;
    emr.emrtext.reference.Set(lround(x), lround(y));
    emr.emrtext.offString = 0;//fill in when serializing
    emr.emrtext.options = 0;
    emr.emrtext.rect.Set(0,0,0,0);
    emr.emrtext.offDx = 0;
    emr.emrtext.str = iConvUTF8toUTF16LE(str);
    emr.emrtext.nChars = emr.emrtext.str.length()/2;
    x_WriteRcd(emr);
}


static
Rboolean EMFDeviceDriver(pDevDesc dd, const char *filename, 
                         const char *bg, const char *fg,
                         double width, double height, double pointsize,
                         const char *family, bool customLty)
{
    CDevEMF *emf;

    if (!(emf = new CDevEMF(customLty, family))) {
	return FALSE;
    }
    dd->deviceSpecific = (void *) emf;

    dd->startfill = R_GE_str2col(bg);
    dd->startcol = R_GE_str2col(fg);
    dd->startps = floor(pointsize);//floor to maintain compat. w/ devPS.c
    dd->startlty = 0;
    dd->startfont = 1;
    dd->startgamma = 1;

    /* Device callbacks */
    dd->activate = EMFcb::Activate;
    dd->deactivate = EMFcb::Deactivate;
    dd->close = EMFcb::Close;
    dd->clip = EMFcb::Clip;
    dd->size = EMFcb::Size;
    dd->newPage = EMFcb::NewPage;
    dd->line = EMFcb::Line;
    dd->text = EMFcb::Text;
    dd->strWidth = EMFcb::StrWidth;
    dd->rect = EMFcb::Rect;
    dd->circle = EMFcb::Circle;
    dd->raster = EMFcb::Raster;
    dd->cap = EMFcb::Cap;
    dd->path = EMFcb::Path;
    dd->polygon = EMFcb::Polygon;
    dd->polyline = EMFcb::Polyline;
    dd->locator = EMFcb::Locator;
    dd->mode = EMFcb::Mode;
    dd->metricInfo = EMFcb::MetricInfo;
    dd->hasTextUTF8 = TRUE;
    dd->textUTF8       = EMFcb::TextUTF8;
    dd->strWidthUTF8   = EMFcb::StrWidth;
    dd->wantSymbolUTF8 = TRUE;
    dd->useRotatedTextInContour = TRUE;
    dd->canClip = FALSE;
    dd->canHAdj = 1;
    dd->canChangeGamma = FALSE;
    dd->displayListOn = FALSE;

    /* Screen Dimensions in device coordinates */
    dd->left = 0;
    dd->right = CDevEMF::x_Inches2Dev(width);
    dd->bottom = 0;
    dd->top = CDevEMF::x_Inches2Dev(height);

    /* Base Pointsize */
    /* Nominal Character Sizes in device units */
    dd->cra[0] = CDevEMF::x_Inches2Dev(0.9 * pointsize/72);
    dd->cra[1] = CDevEMF::x_Inches2Dev(1.2 * pointsize/72);

    /* Character Addressing Offsets */
    /* These offsets should center a single */
    /* plotting character over the plotting point. */
    /* Pure guesswork and eyeballing ... */
    dd->xCharOffset =  0.4900;
    dd->yCharOffset =  0.3333;
    dd->yLineBias = 0.4; /*0.1;*/

    /* Inches per device unit */
    dd->ipr[0] = dd->ipr[1] = 1./CDevEMF::x_Inches2Dev(1);


    if (!emf->Open(filename, dd->right, dd->top)) 
	return FALSE;

    return TRUE;
}

/*  EMF Device Driver Parameters
 *  --------------------
 *  file    = output filename
 *  bg	    = background color
 *  fg	    = foreground color
 *  width   = width in inches
 *  height  = height in inches
 *  pointsize = default font size in points
 *  family  = default font family
 *  userLty = whether to use custom ("user") line textures
 */
extern "C" {
SEXP devEMF(SEXP args)
{
    pGEDevDesc dd;
    const char *file, *bg, *fg, *family;
    double height, width, pointsize;
    Rboolean userLty;

    args = CDR(args); /* skip entry point name */
    file = Rf_translateChar(Rf_asChar(CAR(args))); args = CDR(args);
    bg = CHAR(Rf_asChar(CAR(args)));   args = CDR(args);
    fg = CHAR(Rf_asChar(CAR(args)));   args = CDR(args);
    width = Rf_asReal(CAR(args));	     args = CDR(args);
    height = Rf_asReal(CAR(args));	     args = CDR(args);
    pointsize = Rf_asReal(CAR(args));	     args = CDR(args);
    family = CHAR(Rf_asChar(CAR(args)));     args = CDR(args);
    userLty = (Rboolean) Rf_asLogical(CAR(args));     args = CDR(args);

    R_CheckDeviceAvailable();
    BEGIN_SUSPEND_INTERRUPTS {
	pDevDesc dev;
	if (!(dev = (pDevDesc) calloc(1, sizeof(DevDesc))))
	    return 0;
	if(!EMFDeviceDriver(dev, file, bg, fg, width, height, pointsize,
                            family, userLty)) {
	    free(dev);
	    Rf_error("unable to start %s() device", "emf");
	}
	dd = GEcreateDevDesc(dev);
	GEaddDevice2(dd, "emf");
    } END_SUSPEND_INTERRUPTS;
    return R_NilValue;
}

    const R_ExternalMethodDef ExtEntries[] = {
	{"devEMF", (DL_FUNC)&devEMF, 8},
	{NULL, NULL, 0}
    };
    void R_init_EMF(DllInfo *dll) {
	R_registerRoutines(dll, NULL, NULL, NULL, ExtEntries);
	R_useDynamicSymbols(dll, FALSE);
    }

} //end extern "C"
