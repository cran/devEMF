/* $Id: devEMF.cpp 340 2017-04-14 19:51:31Z pjohnson $
    --------------------------------------------------------------------------
    Add-on package to R to produce EMF graphics output (for import as
    a high-quality vector graphic into Microsoft Office or OpenOffice).


    Copyright (C) 2011-2015 Philip Johnson

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
        MAKEFLAGS="CPPFLAGS=`pkg-config --cflags xft` PKG_LIBS=`pkg-config --libs xft`" R CMD SHLIB '*.cpp'

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
#include <sstream>
//#include <iostream> //FIXME / DEBUG ONLY

#include "fontmetrics.h" //platform-specific font metric code
#include "emf.h"  //defines EMF data structures
#include "emf+.h" //defines EMF+ data structures

using namespace std;


class CDevEMF {
public:
    CDevEMF(const char *defaultFontFamily, int coordDPI, bool customLty,
            bool emfPlus, bool emfpFont, bool emfpRaster) :
        m_debug(false) {
        m_DefaultFontFamily = defaultFontFamily;
        m_PageNum = 0;
        m_NumRecords = 0;
        m_CurrHadj = m_CurrTextCol = m_CurrPolyFill = -1;
        m_CoordDPI = coordDPI;
        //feature options
        m_UseCustomLty = customLty;
        m_UseEMFPlus = emfPlus;
        if (m_debug) Rprintf("using emfplus: %d\n", emfPlus);
        m_UseEMFPlusFont = emfpFont;
        m_UseEMFPlusRaster = emfpRaster;
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
    void Path(double *x, double *y, int nPoly, int *nPts, bool winding,
              const pGEcontext gc);
    void Raster(unsigned int* data, int w, int h, double x, double y,
                double width, double height, double rot,
                Rboolean interpolate);

    // helper functions
    int Inches2Dev(double inches) { return m_CoordDPI*inches;}
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

    unsigned char x_GetPen(const pGEcontext gc) {
        return m_UseEMFPlus ?
            m_ObjectTable.GetPen(gc->col, gc->lwd*72./96., gc->lty, gc->lend,
                                 gc->ljoin, gc->lmitre, Inches2Dev(1)/72.,
                                 m_UseCustomLty, m_File) :
            m_ObjectTableEMF.GetPen(gc->col, gc->lwd*72./96., gc->lty, gc->lend,
                                    gc->ljoin, gc->lmitre, Inches2Dev(1)/72.,
                                    m_UseCustomLty, m_File);
    }
    unsigned char x_GetFont(const pGEcontext gc, SSysFontInfo **info=NULL,
                            const char *fontfamily = NULL) {
        int face = (gc->fontface < 1  || gc->fontface > 4) ? 1 : gc->fontface;
        const char *family = fontfamily ? fontfamily :
            gc->fontfamily[0] != '\0' ? gc->fontfamily :
            m_DefaultFontFamily.c_str();
        //emf+ font support is incomplete in LibreOffice (rotations are buggy)
        return m_UseEMFPlus  &&  m_UseEMFPlusFont ?
            m_ObjectTable.GetFont(face, Inches2Dev(x_EffPointsize(gc)/72),
                                  family, iConvUTF8toUTF16LE(family),
                                  m_File, info) :
            m_ObjectTableEMF.GetFont(face, Inches2Dev(x_EffPointsize(gc)/72),
                                     family, iConvUTF8toUTF16LE(family),
                                     m_File, info);
    }

private:
    bool m_debug;
    EMF::ofstream m_File;
    int m_NumRecords;
    int m_PageNum;
    int m_Width, m_Height;
    string m_DefaultFontFamily;
    int m_CoordDPI;
    bool m_UseCustomLty;
    bool m_UseEMFPlus;
    bool m_UseEMFPlusFont;
    bool m_UseEMFPlusRaster;

    //EMF states
    double m_CurrHadj;
    int m_CurrTextCol;
    int m_CurrPolyFill;

    //EMF/EMF+ objects
    EMFPLUS::CObjectTable m_ObjectTable;
    EMF::CObjectTable m_ObjectTableEMF;
};

// R callbacks below (declare extern "C")
namespace EMFcb {
    extern "C" {
        void Activate(pDevDesc) {}
        void Deactivate(pDevDesc) {}
        void Mode(int, pDevDesc) {}
        Rboolean Locator(double*, double*, pDevDesc) {return FALSE;}
        void Raster(unsigned int* r, int w, int h, double x, double y,
                    double width, double height, double rot,
                    Rboolean interpolate, const pGEcontext, pDevDesc dd) {
            static_cast<CDevEMF*>(dd->deviceSpecific)->
                Raster(r,w,h,x,y,width,height,rot,interpolate);
        }
        SEXP Cap(pDevDesc) {
            Rf_warning("Raster capture not available for EMF");
            return R_NilValue;
        }
        void Path(double* x, double* y, int n, int* np, Rboolean wnd,
                  const pGEcontext gc, pDevDesc dd) {
            static_cast<CDevEMF*>(dd->deviceSpecific)->Path(x,y, n,np, wnd, gc);
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


void CDevEMF::MetricInfo(int c, const pGEcontext gc, double* ascent,
                     double* descent, double* width)
{
    if (m_debug) Rprintf("metricinfo: %i %x (face %i)\n",c,abs(c),gc->fontface);
    //cout << gc->fontfamily << "/" << gc->fontface << " -- " << c << " / " << (char) c;
    if (c < 0) { c = -c; }

    SSysFontInfo *info;
    x_GetFont(gc, &info);
    if (!(info  &&  info->HasChar(c))  &&  gc->fontface == 5) {
        // check for char existence in "Symbol" font
        x_GetFont(gc, &info, "Symbol");
    } else if (!info) {
        //last ditch attempt
        x_GetFont(gc, &info, "sans");
        if (info) {
            Rf_warning("Using 'sans' font metrics instead of requested '%s'",
                       gc->fontfamily);
        }
    }
    if (!info) { //no metrics available
        *ascent = 0;
        *descent = 0;
        *width = 0;
    } else {
        info->GetMetrics(c, *ascent, *descent, *width);
    }
    if (m_debug) Rprintf("\t%f/%f/%f\n",*ascent,*descent,*width);
}


double CDevEMF::StrWidth(const char *str, const pGEcontext gc) {
    if (m_debug) Rprintf("strwidth ('%s') --> ", str);

    SSysFontInfo *info;
    x_GetFont(gc, &info);

    double width =  info ? info->GetStrWidth(str) : 0;

    if (m_debug) Rprintf("%f\n", width);
    //cout << "strwidth: " << width/Inches2Dev(1) << endl;
    return width;
}

	/* Initialize the device */

bool CDevEMF::Open(const char* filename, int width, int height)
{
    if (m_debug) Rprintf("open: %i, %i\n", width, height);
    m_Width = width;
    m_Height = height;
    
    m_File.open(R_ExpandFileName(filename), ios_base::binary);
    if (!m_File) {
	return FALSE;
    }

    {
        EMF::SHeader emr;
        emr.bounds.Set(0,0,m_Width,m_Height); //device units
        emr.frame.Set(0,0, // units of 0.01mm
                      2540*m_Width/Inches2Dev(1),
                      2540*m_Height/Inches2Dev(1)); 
        emr.signature = 0x464D4520;
        emr.version = 0x00010000;
        emr.nBytes = 0;   //WILL EDIT WHEN CLOSING
        emr.nRecords = 0; //WILL EDIT WHEN CLOSING
        emr.nHandles = 0; //WILL EDIT WHEN CLOSING
        emr.reserved = 0x0000;
        string ver = "?"; // devEMF version
        {//find version using R "packageVersion" function
            SEXP packageVer, call, res;
            PROTECT(packageVer = Rf_findFun(Rf_install("packageVersion"),
                                            R_GlobalEnv));
            PROTECT(call = Rf_lang2(packageVer, Rf_ScalarString
                                    (Rf_mkChar("devEMF"))));
            PROTECT(res = Rf_eval(call, R_GlobalEnv));
            if (Rf_isVector(res)  &&  Rf_length(res) == 1  &&
                Rf_isInteger(VECTOR_ELT(res,0))  &&
                Rf_length(VECTOR_ELT(res,0)) >= 1) {
                std::ostringstream oss;
                oss << INTEGER(VECTOR_ELT(res,0))[0];
                if (Rf_length(VECTOR_ELT(res,0)) >= 2) {
                    oss << "." << INTEGER(VECTOR_ELT(res,0))[1];
                    if (Rf_length(VECTOR_ELT(res,0)) >= 3) {
                        oss << "." << INTEGER(VECTOR_ELT(res,0))[2];
                    }
                }
                ver = oss.str();
            }
            UNPROTECT(3);
        }
        //Description string must be UTF-16LE
        emr.desc = iConvUTF8toUTF16LE("Created by R using devEMF ver. "+ver);
        emr.nDescription = emr.desc.length()/2;
        emr.offDescription = 0; //set during serialization
        emr.nPalEntries = 0;
        emr.device.Set(m_Width,m_Height);
        emr.millimeters.Set(m_Width * (25.4/Inches2Dev(1)),
                            m_Height * (25.4/Inches2Dev(1)));
        emr.cbPixelFormat=0x00000000;
        emr.offPixelFormat=0x00000000;
        emr.bOpenGL=0x00000000;
        emr.micrometers.Set(m_Width * (25400/Inches2Dev(1)),
                            m_Height * (25400/Inches2Dev(1)));
        emr.Write(m_File);
    }

    if (m_UseEMFPlus) {
        {
            EMFPLUS::SHeader emr;
            emr.plusFlags = 0; //specifies for video display context
            emr.dpiX = Inches2Dev(1);
            emr.dpiY = Inches2Dev(1);
            emr.Write(m_File);
        }
        { // page transform (1 pixel == 1 device unit)
            EMFPLUS::SSetPageTransform emr(EMFPLUS::eUnitPixel, 1);
            emr.Write(m_File);
        }
        {// use decent anti-aliasing (key for viewing in MS Office)
            EMFPLUS::SSetAntiAliasMode emr(true);
            emr.Write(m_File);
        }
        {// (unclear if affects anything with either MS or Libre Office)
            EMFPLUS::SSetTextRenderingHint emr(EMFPLUS::eTRHAntiAliasGridFit);
            emr.Write(m_File);
        }
    }

    if (!m_UseEMFPlus  ||  !m_UseEMFPlusFont  ||  !m_UseEMFPlusRaster) {
        {// page transform (1 pixel == 1 device unit)
            EMF::S_SETMAPMODE emr;
            emr.mode = EMF::eMM_TEXT;
            emr.Write(m_File);
        }
        
        {//Don't paint text box background
            EMF::S_SETBKMODE emr;
            emr.mode = EMF::eTRANSPARENT;
            emr.Write(m_File);
        }
     
        {//Fixed text alignment point
            EMF::S_SETTEXTALIGN emr;
            emr.mode = EMF::eTA_BASELINE|EMF::eTA_LEFT;
            emr.Write(m_File);
        }
    }
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
    if (m_debug) Rprintf("clip %f,%f,%f,%f\n", x0,y0,x1,y1);
    return;
}


void CDevEMF::Close(void)
{
    if (m_debug) Rprintf("close\n");

    if (m_UseEMFPlus) {
        EMFPLUS::SEndOfFile empr;
        empr.Write(m_File);
    }

    { //EOF record
        EMF::S_EOF emr;
        emr.nPalEntries = 0;
        emr.offPalEntries = 0;
        emr.nSizeLast = sizeof(emr);
        emr.Write(m_File);
    }
    

    { //Edit header record to report number of records, handles & size
        unsigned int nBytes = m_File.tellp();
        m_File.seekp(4*12);//offset of nBytes field of EMF header
        string data;
        data << EMF::TUInt4(nBytes)
             << EMF::TUInt4(m_File.nRecords)
            //not mentioned in spec, but seems to need one extra handle
             << EMF::TUInt4(m_ObjectTableEMF.GetSize()+1);
        m_File.write(data.data(), 12);
        m_File.close();
    }
}

void CDevEMF::Raster(unsigned int* r, int w, int h, double x, double y,
                     double width, double height, double rot,
                     Rboolean interpolate) {
    if (m_debug) Rprintf("raster: %d,%d / %f,%f,%f,%f\n", w,h,x,y,width,height);
    
    x_TransformY(&y, 1);//EMF has origin in upper left; R in lower left
    y -= height;
    /* Sigh.. as of 2016, LibreOffice support for EMF+ raster ops is broken/missing .*/
    if (m_UseEMFPlus  &&  m_UseEMFPlusRaster) {
        if (rot != 0) {
            EMFPLUS::SMultiplyWorldTransform trans
                (cos(rot*M_PI/180), -sin(rot*M_PI/180),
                 sin(rot*M_PI/180), cos(rot*M_PI/180),
                 x, y+height);
            trans.Write(m_File);
            x = 0; y = -height; //rotate around ll corner
        }
        EMFPLUS::SSetInterpolationMode m1
            (interpolate ?
             EMFPLUS::eInterpolationModeHighQualityBilinear:
             EMFPLUS::eInterpolationModeNearestNeighbor);
        m1.Write(m_File);
        EMFPLUS::SSetPixelOffsetMode m2(EMFPLUS::ePixelOffsetModeHalf);
        m2.Write(m_File);
        EMFPLUS::SDrawImage image(m_ObjectTable.GetImage(r,w,h,m_File), w,h,
                                  x, y, width, height);
        image.Write(m_File);
        if (rot != 0) {
            EMFPLUS::SResetWorldTransform trans;
            trans.Write(m_File);
        }
    } else {
        /* Unfortunately, I can't figure out a interpolation control
           for EMF -- so this seems to leave it up to the client
           program (generally seems sort of linear interpolation) The
           following lines look like they might work.. but don't
        if (!interpolate) {
           EMF::S_SETSTRETCHBLTMODE m1(3);
           m1.Write(m_File);
           EMF::S_SETBRUSHORGEX m2(0,0);
           m2.Write(m_File);
        }
        */
        if (rot != 0) {
            EMF::S_SAVEDC emr1;
            emr1.Write(m_File);
            EMF::S_MODIFYWORLDTRANSFORM emr2(EMF::eMWT_LEFTMULTIPLY);
            emr2.xform.Set(cos(rot*M_PI/180), -sin(rot*M_PI/180),
                           sin(rot*M_PI/180), cos(rot*M_PI/180),
                          x, y+height);
            emr2.Write(m_File);
            x = 0; y = -height; //rotate around ll corner
        }
        EMF::S_STRETCHBLT bmp(r, w,h,x,y,width,height);
        bmp.Write(m_File);
        if (rot != 0) {
            EMF::S_RESTOREDC emr;
            emr.Write(m_File);
        }
    }
}


void CDevEMF::Line(double x1, double y1, double x2, double y2,
	       const pGEcontext gc)
{
    if (m_debug) Rprintf("line\n");

    if (x1 != x2  ||  y1 != y2) {
	double x[2], y[2];
        x[0] = x1; x[1] = x2;
        y[0] = y1; y[1] = y2;
        Polyline(2, x, y, gc);
    }
}

void CDevEMF::Polyline(int n, double *x, double *y, const pGEcontext gc)
{
    if (m_debug) Rprintf("polyline\n");

    x_TransformY(y, n);//EMF has origin in upper left; R in lower left
    if (m_UseEMFPlus) {
        EMFPLUS::SDrawLines lines(n, x, y, x_GetPen(gc));
        lines.Write(m_File);
    } else {
        x_GetPen(gc);
        EMF::SPoly polyline(EMF::eEMR_POLYLINE, n, x, y);
        polyline.Write(m_File);
    }
}

void CDevEMF::Rect(double x0, double y0, double x1, double y1, const pGEcontext gc)
{
    if (m_debug) Rprintf("rect (converted to poly)\n");

    double x[4], y[4];
    x[0] = x[1] = x0;
    x[2] = x[3] = x1;
    y[0] = y[3] = y0;
    y[1] = y[2] = y1;
    Polygon(4, x, y, gc);
}

void CDevEMF::Circle(double x, double y, double r, const pGEcontext gc)
{
    if (m_debug) Rprintf("circle (%f,%f r=%f)\n", x, y,r);

    x_TransformY(&y, 1);//EMF has origin in upper left; R in lower left
    if (m_UseEMFPlus) {
        {
            EMFPLUS::SDrawEllipse circle(x-r, y-r, 2*r, 2*r, x_GetPen(gc));
            circle.Write(m_File);
        }
        if (!R_TRANSPARENT(gc->fill)) {
            EMFPLUS::SFillEllipse circle(x-r, y-r, 2*r, 2*r, gc->fill);
            circle.Write(m_File);
        }
    } else {
        x_GetPen(gc);
        m_ObjectTableEMF.GetBrush(gc->fill, m_File);
        EMF::S_ELLIPSE emr;
        emr.box.Set(floor(x-r + 0.5), floor(y-r + 0.5),
                    floor(x+r + 0.5), floor(y+r + 0.5));
        emr.Write(m_File);
    }
}

void CDevEMF::Polygon(int n, double *x, double *y, const pGEcontext gc)
{
    if (m_debug) { Rprintf("polygon"); for (int i = 0; i<n;  ++i) {Rprintf("(%f,%f) ", x[i], y[i]);}; Rprintf("\n");}

    x_TransformY(y, n);//EMF has origin in upper left; R in lower left
    if (m_UseEMFPlus) {
        int pathId = m_ObjectTable.GetPath(1,x,y,&n, m_File);
        if (!R_TRANSPARENT(gc->fill)) {
            EMFPLUS::SFillPath fill(pathId, gc->fill);
            fill.Write(m_File);
        }
        {
            EMFPLUS::SDrawPath drawPath(pathId, x_GetPen(gc));
            drawPath.Write(m_File);
        }
    } else {
        x_GetPen(gc);
        m_ObjectTableEMF.GetBrush(gc->fill, m_File);
        EMF::SPoly polygon(EMF::eEMR_POLYGON, n, x, y);
        polygon.Write(m_File);
    }
}

void CDevEMF::Path(double *x, double *y, int nPoly, int *nPts, bool winding,
                   const pGEcontext gc)
{
    if (m_debug) { Rprintf("path\t(%d subpaths w/ %i winding)", nPoly, winding?1:0); }

    int n = 0;
    for (int i = 0;  i < nPoly;  ++i) {
        n += nPts[i];
    }
    x_TransformY(y, n);//EMF has origin in upper left; R in lower left

    if (m_UseEMFPlus) {
        // I can't find a way to make use of "winding" in EMF+
        int pathId = m_ObjectTable.GetPath(nPoly, x, y, nPts, m_File);
        EMFPLUS::SDrawPath drawPath(pathId, x_GetPen(gc));
        drawPath.Write(m_File);
        if (!R_TRANSPARENT(gc->fill)) {
            EMFPLUS::SFillPath fill(pathId, gc->fill);
            fill.Write(m_File);
        }
    } else {
        Rf_warning("devEMF does not implement 'path' drawing for EMF (only EMF+)");
        /*
        if (( winding  &&  m_CurrPolyFill != EMF::ePF_WINDING)  ||
            (!winding  &&  m_CurrPolyFill != EMF::ePF_ALTERNATE)) {
            m_CurrPolyFill = (winding) ? EMF::ePF_WINDING : EMF::ePF_ALTERNATE;
            EMF::S_SETPOLYFILLMODE emr;
            emr.mode = m_CurrPolyFill;
            emr.Write(m_File);
        }
        */
    }
}

void CDevEMF::TextUTF8(double x, double y, const char *str, double rot,
                       double hadj, const pGEcontext gc)
{
    if (m_debug) Rprintf("textUTF8: %s, %x  at %.1f %.1f\n", str, gc->col, x, y);
    x_TransformY(&y, 1);//EMF has origin in upper left; R in lower left

    SSysFontInfo *info;
    unsigned char fontId = x_GetFont(gc, &info);
    //Sigh.. as of 2016 because LibreOffice EMF+ has buggy support of transformations to fonts (e.g., shrinks instead of rotates!)
    if (m_UseEMFPlus  &&  m_UseEMFPlusFont) {
        if (rot != 0) {
            EMFPLUS::SMultiplyWorldTransform trans
                (cos(rot*M_PI/180), -sin(rot*M_PI/180),
                 sin(rot*M_PI/180), cos(rot*M_PI/180),
                 x, y);
            trans.Write(m_File);
            x = 0; y = 0; //because already translated!
        }
        EMFPLUS::SDrawString text
            (iConvUTF8toUTF16LE(str), gc->col, fontId,
             m_ObjectTable.GetStringFormat(EMFPLUS::eStrAlignNear,
                                           EMFPLUS::eStrAlignNear, m_File));
        double width = info->GetStrWidth(str);
        text.m_LayoutRect.x = x - width*hadj;
        double ascent, descent;
        info->GetFontBBox(ascent, descent, width);
        if (m_debug) Rprintf("fbbox: %.1f %.1f %.1f\n", ascent, descent, width);
        text.m_LayoutRect.y = y - ascent;//find baseline
        text.Write(m_File);
        if (rot != 0) {
            EMFPLUS::SResetWorldTransform trans;
            trans.Write(m_File);
        }
    } else {
        if (rot != 0) {
            EMF::S_SAVEDC emr1;
            emr1.Write(m_File);
            EMF::S_MODIFYWORLDTRANSFORM emr2(EMF::eMWT_LEFTMULTIPLY);
            emr2.xform.Set(cos(rot*M_PI/180), -sin(rot*M_PI/180),
                           sin(rot*M_PI/180), cos(rot*M_PI/180),
                          x, y);
            emr2.Write(m_File);
            x = 0; y = 0; //because already translated!
        }

        if (m_CurrTextCol != gc->col) {
            EMF::S_SETTEXTCOLOR emr;
            emr.color.Set(R_RED(gc->col), R_GREEN(gc->col), R_BLUE(gc->col));
            if (R_ALPHA(gc->col) > 0  &&  R_ALPHA(gc->col) < 255) {
                Rf_warning("Unfortunately, partial transparency for fonts is not supported by EMF, and Libreoffice EMF+ support for fonts is incomplete; thus for now, devEMF does not enable transparent fonts.");
            }
            emr.Write(m_File);
            m_CurrTextCol = gc->col;
        }
        EMF::S_EXTTEXTOUTW emr;
        emr.bounds.Set(0,0,0,0);//EMF spec says to ignore
        emr.graphicsMode = EMF::eGM_ADVANCED;
        emr.exScale = 0; //not used with GM_ADVANCED
        emr.eyScale = 0; //not used with GM_ADVANCED
        double textWidth =  info ? info->GetStrWidth(str) : 0;
        emr.emrtext.reference.Set(x-floor(textWidth*hadj + 0.5), y);//manually shift reference point for LibreOffice + tranformation compatibility
        emr.emrtext.offString = 0; // fill in when serializing
        emr.emrtext.options = 0; // from spec, seems should be eETO_NO_RECT, but office does seem to support this
        emr.emrtext.rect.Set(0,0,0,0);
        emr.emrtext.offDx = 0; //0 when not included (spec ambiguous but see https://social.msdn.microsoft.com/Forums/en-US/29e46348-c2eb-44d5-8d1a-47c1ecdc68ff/msemf-emrtextdxbuffer-is-optional-how-to-specify-its-not-specified?forum=os_windowsprotocols)
        emr.emrtext.str = iConvUTF8toUTF16LE(str);
        emr.emrtext.nChars = emr.emrtext.str.length()/2;
        emr.Write(m_File);

        if (rot != 0) {
            EMF::S_RESTOREDC emr;
            emr.Write(m_File);
        }
    }
}


static
Rboolean EMFDeviceDriver(pDevDesc dd, const char *filename, 
                         const char *bg, const char *fg,
                         double width, double height, double pointsize,
                         const char *family, int coordDPI, bool customLty,
                         bool emfPlus, bool emfpFont, bool emfpRaster)
{
    CDevEMF *emf;

    if (!(emf = new CDevEMF(family, coordDPI, customLty, emfPlus, emfpFont,
                            emfpRaster))){
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
#if R_GE_version >= 6
    dd->raster = EMFcb::Raster;
    dd->cap = EMFcb::Cap;
#endif
#if R_GE_version >= 8
    dd->path = EMFcb::Path;
#endif
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
    dd->right = emf->Inches2Dev(width);
    dd->bottom = 0;
    dd->top = emf->Inches2Dev(height);

    /* Base Pointsize */
    /* Nominal Character Sizes in device units */
    dd->cra[0] = emf->Inches2Dev(0.9 * pointsize/72);
    dd->cra[1] = emf->Inches2Dev(1.2 * pointsize/72);

    /* Character Addressing Offsets */
    /* These offsets should center a single */
    /* plotting character over the plotting point. */
    /* Pure guesswork and eyeballing ... */
    dd->xCharOffset =  0.4900;
    dd->yCharOffset =  0.3333;
    dd->yLineBias = 0.2;

    /* Inches per device unit */
    dd->ipr[0] = dd->ipr[1] = 1./emf->Inches2Dev(1);


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
 *  emfPlus = whether to use EMF+ format
 *  emfpFont = whether to use EMF+ text records
 *  emfpRaster = whether to use EMF+ raster records
 */
extern "C" {
SEXP devEMF(SEXP args)
{
    pGEDevDesc dd;
    const char *file, *bg, *fg, *family;
    double height, width, pointsize;
    Rboolean userLty, emfPlus, emfpFont, emfpRaster;
    int coordDPI;

    args = CDR(args); /* skip entry point name */
    file = Rf_translateChar(Rf_asChar(CAR(args))); args = CDR(args);
    bg = CHAR(Rf_asChar(CAR(args)));   args = CDR(args);
    fg = CHAR(Rf_asChar(CAR(args)));   args = CDR(args);
    width = Rf_asReal(CAR(args));	     args = CDR(args);
    height = Rf_asReal(CAR(args));	     args = CDR(args);
    pointsize = Rf_asReal(CAR(args));	     args = CDR(args);
    family = CHAR(Rf_asChar(CAR(args)));     args = CDR(args);
    coordDPI = Rf_asInteger(CAR(args));     args = CDR(args);
    userLty = (Rboolean) Rf_asLogical(CAR(args));     args = CDR(args);
    emfPlus = (Rboolean) Rf_asLogical(CAR(args));     args = CDR(args);
    emfpFont = (Rboolean) Rf_asLogical(CAR(args));     args = CDR(args);
    emfpRaster = (Rboolean) Rf_asLogical(CAR(args));     args = CDR(args);

    R_CheckDeviceAvailable();
    BEGIN_SUSPEND_INTERRUPTS {
	pDevDesc dev;
	if (!(dev = (pDevDesc) calloc(1, sizeof(DevDesc))))
	    return 0;
	if(!EMFDeviceDriver(dev, file, bg, fg, width, height, pointsize,
                            family, coordDPI, userLty, emfPlus, emfpFont,
                            emfpRaster)) {
	    free(dev);
	    Rf_error("unable to start %s() device", "emf");
	}
	dd = GEcreateDevDesc(dev);
	GEaddDevice2(dd, "emf");
    } END_SUSPEND_INTERRUPTS;
    return R_NilValue;
}

    const R_ExternalMethodDef ExtEntries[] = {
	{"devEMF", (DL_FUNC)&devEMF, 12},
	{NULL, NULL, 0}
    };
    void R_init_devEMF(DllInfo *dll) {
	R_registerRoutines(dll, NULL, NULL, NULL, ExtEntries);
	R_useDynamicSymbols(dll, FALSE);
    }

} //end extern "C"
