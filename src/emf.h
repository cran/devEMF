/* $Id: emf.h 306 2015-01-29 18:45:54Z pjohnson $
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


    Note this header file is C++ (R policy requires that all headers
    end with .h).
    --------------------------------------------------------------------------
*/

#include <cassert>
#include <string>

// structs for EMF
namespace EMF {
    enum ERecordType {
        eEMR_HEADER = 1,
        eEMR_POLYGON = 3,
        eEMR_POLYLINE = 4,
        eEMR_EOF = 14,
        eEMR_SETMAPMODE = 17,
        eEMR_SETBKMODE = 18,
        eEMR_SETTEXTALIGN = 22,
        eEMR_SETTEXTCOLOR = 24,
        eEMR_SELECTOBJECT = 37,
        eEMR_CREATEPEN = 38,
        eEMR_CREATEBRUSHINDIRECT = 39,
        eEMR_ELLIPSE = 42,
        eEMR_RECTANGLE = 43,
        eEMR_SETMITERLIMIT = 58,
        eEMR_EXTCREATEFONTINDIRECTW = 82,
        eEMR_EXTTEXTOUTW = 84,
        eEMR_EXTCREATEPEN = 95
    };

    enum EPenStyle {
        ePS_ENDCAP_ROUND  = 0x00000000,
        ePS_JOIN_ROUND    = 0x00000000,
        ePS_SOLID         = 0x00000000,
        ePS_DASH          = 0x00000001,
        ePS_DOT           = 0x00000002,
        ePS_DASHDOT       = 0x00000003,
        ePS_DASHDOTDOT    = 0x00000004,
        ePS_NULL          = 0x00000005,
        ePS_INSIDEFRAME   = 0x00000006,
        ePS_USERSTYLE     = 0x00000007,
        ePS_ALTERNATE     = 0x00000008,
        ePS_ENDCAP_SQUARE = 0x00000100,
        ePS_ENDCAP_FLAT   = 0x00000200,
        ePS_JOIN_BEVEL    = 0x00001000,
        ePS_JOIN_MITER    = 0x00002000,
        ePS_GEOMETRIC     = 0x00010000
    };

    enum EBrushStyle {
        eBS_SOLID = 0,
        eBS_NULL  = 1
    };
    
    enum EFontCharset {
        eDEFAULT_CHARSET = 1,
        eSYMBOL_CHARSET  = 2
    };

    enum EFontOutPrecision {
        eOUT_DEFAULT_PRECIS   = 0,
        eOUT_STRING_PRECIS    = 1,
        eOUT_CHARACTER_PRECIS = 2,
        eOUT_STROKE_PRECIS    = 3
    };

    enum EFontClipPrecision {
        eCLIP_DEFAULT_PRECIS    = 0x00,
        eCLIP_CHARACTER_PRECIS  = 0x01,
        eCLIP_STROKE_PRECIS     = 0x02
    };

    enum EFontQuality {
        eDEFAULT_QUALITY       = 0,
        eDRAFT_QUALITY         = 1,
        ePROOF_QUALITY         = 2,
        eNONANTIALIASED_QUALITY= 3,
        eANTIALIASED_QUALITY   = 4,
        eCLEARTYPE_QUALITY     = 5
    };

    enum EFontPitchFamily {
        eDEFAULT_PITCH      = 0x00,
        eFIXED_PITCH        = 0x01,
        eVARIABLE_PITCH     = 0x02,

        eFF_DONTCARE        = 0x00,
        eFF_ROMAN           = 0x10,
        eFF_SWISS           = 0x20,
        eFF_MODERN          = 0x30,
        eFF_SCRIPT          = 0x40,
        eFF_DECORATIVE      = 0x50
    };

    enum ETextAlign {
        eTA_LEFT            = 0x00,
        eTA_TOP             = 0x00,
        eTA_RIGHT           = 0x02,
        eTA_CENTER          = 0x06,
        eTA_BOTTOM          = 0x08,
        eTA_BASELINE        = 0x18
    };

    enum EBkMode {
        eTRANSPARENT      = 1,
        eOPAQUE           = 2
    };

    enum EMapMode {
        eMM_TEXT         = 1,
        eMM_LOMETRIC	 = 2,
        eMM_HIMETRIC	 = 3,
        eMM_LOENGLISH	 = 4,
        eMM_HIENGLISH	 = 5,
        eMM_TWIPS	 = 6,
        eMM_ISOTROPIC	 = 7,
        eMM_ANISOTROPIC	 = 8
    };

    enum EGraphicsMode {
        eGM_COMPATIBLE    = 1,
        eGM_ADVANCED      = 2
    };

    // ------------------------------------------------------------------------
    // Generic objects for specified bytes of little-endian data storage

    template<typename TType, size_t nBytes> class CLEType {
    public:
        char m_Val[nBytes];

        CLEType(void) {}
        CLEType(TType v) { assert(sizeof(v) >= nBytes); *this = v; }
        CLEType& operator= (TType v) {
            //store as little-endian
            unsigned char *ch = reinterpret_cast<unsigned char*>(&v);
            for (unsigned int i = 0;  i < nBytes;  ++i) {
#ifdef WORDS_BIGENDIAN
                m_Val[i] = ch[sizeof(TType) - i - 1];
#else
                m_Val[i] = ch[i];
#endif
            }
            //make sure we get signed bit if sizeof(TType) > nBytes
            if (sizeof(TType) > nBytes) {
#ifdef WORDS_BIGENDIAN
                m_Val[nBytes-1] |= ch[0] & 0x80;
#else
                m_Val[nBytes-1] |= ch[sizeof(TType) - 1] & 0x80;
#endif
            }
            return *this;
        }

        friend std::string& operator<< (std::string &o, const CLEType &d) {
            o.append(d.m_Val, nBytes);
            return o;
        }
    };
    typedef CLEType<unsigned int,   4> TUInt4;
    typedef CLEType<unsigned short, 2> TUInt2;
    typedef CLEType<unsigned char,  1> TUInt1;
    typedef CLEType<int, 4>   TInt4;
    typedef CLEType<float, 4> TFloat4;

    // ------------------------------------------------------------------------
    // EMF Objects used repeatedly

    struct SPoint {
        int x, y;
        void Set(int xx, int yy) { x = xx; y = yy; }
        friend std::string& operator<< (std::string &o, const SPoint &d) {
            return o << TInt4(d.x) << TInt4(d.y);
        }
    };

    struct SSize {
        unsigned int cx, cy;
        void Set(unsigned int xx, unsigned int yy) { cx = xx; cy = yy; }
        friend std::string& operator<< (std::string &o, const SSize &d) {
            return o << TUInt4(d.cx) << TUInt4(d.cy);
        }
    };
    
    struct SRect {
        int left, top, right, bottom;
        void Set(int l, int t, int r, int b) {
            left = l; top = t; right = r; bottom = b;
        }
        friend std::string& operator<< (std::string &o, const SRect &d) {
            return o << TInt4(d.left) << TInt4(d.top)
                     << TInt4(d.right) << TInt4(d.bottom);
        }
    };

    struct SColorRef {
        TUInt1 red, green, blue, reserved;
        void Set(unsigned char r, unsigned char g, unsigned char b) {
            red = r;  green = g;  blue = b; reserved=0x00;
        }
        friend std::string& operator<< (std::string &o, const SColorRef &d) {
            //last byte is "reserved"
            return o << d.red << d.green << d.blue << d.reserved;
        }
    };

    // ------------------------------------------------------------------------
    // EMF Records + EMF objects used in only one record begin here

    struct SRecord {
        const TUInt4 iType;
        TUInt4 nSize;
    SRecord(unsigned int t) : iType(t), nSize(0) {}
    };

    struct SHeader : SRecord {
        SRect bounds;
        SRect frame;
        unsigned int signature;
        unsigned int version;
        unsigned int nBytes;
        unsigned int nRecords;
        unsigned short nHandles;
        unsigned short reserved;
        unsigned int nDescription;
        unsigned int offDescription;
        unsigned int nPalEntries;
        SSize device;
        SSize millimeters;
        unsigned int cbPixelFormat;
        unsigned int offPixelFormat;
        unsigned int bOpenGL;
        SSize micrometers;
        std::string desc;
        SHeader(void) : SRecord(eEMR_HEADER) {}
        void Serialize(std::string &o) const {
            o << iType << nSize << bounds << frame << TUInt4(signature) << TUInt4(version) << TUInt4(nBytes) << TUInt4(nRecords) << TUInt2(nHandles) << TUInt2(reserved) << TUInt4(nDescription) << TUInt4(108) << TUInt4(nPalEntries) << device << millimeters << TUInt4(cbPixelFormat) << TUInt4(offPixelFormat) << TUInt4(bOpenGL) << micrometers;
            o.append(desc);
        }
    };

    struct SemrText {
        SPoint reference;
        unsigned int  nChars;
        unsigned int  offString;
        unsigned int  options;
        SRect  rect;
        unsigned int  offDx;
        std::string str;
    };
    struct S_EXTTEXTOUTW : SRecord {
        SRect   bounds;
        unsigned int graphicsMode;
        TFloat4  exScale;
        TFloat4  eyScale;
        SemrText emrtext;
        S_EXTTEXTOUTW(void) : SRecord(eEMR_EXTTEXTOUTW) {}
	void Serialize(std::string &o) const {
            o << iType << nSize << bounds << TUInt4(graphicsMode) << exScale << eyScale << emrtext.reference << TUInt4(emrtext.nChars) << TUInt4(19*4) << TUInt4(emrtext.options) << emrtext.rect << TUInt4(emrtext.offDx);
            o.append(emrtext.str);
	}
    };

    struct SLogPenEx {
        unsigned int penStyle;
        TUInt4 width;
        TUInt4 brushStyle;
        SColorRef color;
        TUInt4 brushHatch;
        unsigned int numEntries;
    };
    struct S_EXTCREATEPEN : SRecord {
        unsigned int ihPen;
        TUInt4 offBmi;
        TUInt4 cbBmi;
        TUInt4 offBits;
        TUInt4 cbBits;
        SLogPenEx elp;
        TUInt4 *styleEntries;

        class Less { public: bool operator()(const S_EXTCREATEPEN *p1,
                                             const S_EXTCREATEPEN *p2) const {
	    int res = memcmp(&p1->elp, &p2->elp, sizeof(p1->elp));
            if (res != 0) return res < 0;
            if (p1->elp.numEntries < p2->elp.numEntries) return true;
            if (p1->elp.numEntries > p2->elp.numEntries) return false;
            return memcmp(p1->styleEntries, p2->styleEntries,
                          p1->elp.numEntries * 4) < 0;
	}};
        S_EXTCREATEPEN(void) : SRecord(eEMR_EXTCREATEPEN), styleEntries(NULL) {}
        ~S_EXTCREATEPEN(void) { delete[] styleEntries; }
	void Serialize(std::string &o) const {
            o << iType << nSize << TUInt4(ihPen) << offBmi << cbBmi << offBits << cbBits << TUInt4(elp.penStyle) << elp.width << elp.brushStyle << elp.color << elp.brushHatch << TUInt4(elp.numEntries);
            for (unsigned int i = 0;  i < elp.numEntries;  ++i) {
                o << styleEntries[i];
            }
	}
    };

    struct SLogBrushEx {
        TUInt4    brushStyle;
        SColorRef color;
        TUInt4    brushHatch;
    };
    struct SBrush : SRecord {
        unsigned int ihBrush;
        SLogBrushEx  lb;

	friend bool operator<(const SBrush &a, const SBrush &b) {
	    return memcmp(&a.lb, &b.lb, sizeof(a.lb)) < 0;
	}
	SBrush(void) : SRecord(eEMR_CREATEBRUSHINDIRECT) {}
	void Serialize(std::string &o) const {
            o << iType << nSize << TUInt4(ihBrush) << lb.brushStyle << lb.color << lb.brushHatch;
	}
    };

    enum EFontWeight {
        eFontWeight_normal = 400,
        eFontWeight_bold   = 700
    };
    struct SLogFont {
        TInt4   height;
        TInt4   width;
        TInt4   escapement;
        TInt4   orientation;
        TInt4   weight;
        TUInt1  italic;
        TUInt1  underline;
        TUInt1  strikeOut;
        TUInt1  charSet;
        TUInt1  outPrecision;
        TUInt1  clipPrecision;
        TUInt1  quality;
        TUInt1  pitchAndFamily;
        char    faceName[64]; // <=32 UTF-16 characters
        void SetFace(const std::string &utf16) {
            memset(faceName, 0, 64);//blank out for comparison
            memcpy(faceName, utf16.data(),
                   64 < utf16.length() ? 64 : utf16.length());
        }
    };
    struct SFont : SRecord {
        unsigned int ihFont;
        SLogFont lf;
        
	friend bool operator<(const SFont &f1, const SFont &f2) {
	    return memcmp(&f1.lf, &f2.lf, sizeof(f1.lf)) < 0;
	}
	SFont(void) : SRecord(eEMR_EXTCREATEFONTINDIRECTW) {
            memset(&lf, '\0', sizeof(lf));
        }
	void Serialize(std::string &o) const {
            o << iType << nSize << TUInt4(ihFont) << lf.height << lf.width
              << lf.escapement << lf.orientation << lf.weight
              << lf.italic << lf.underline << lf.strikeOut
              << lf.charSet << lf.outPrecision << lf.clipPrecision
              << lf.quality << lf.pitchAndFamily;
            o.append(lf.faceName, 64);
	}
    };

    struct SPoly : SRecord { //also == POLYLINE or POLYGON
        SRect  bounds;
        unsigned int count;
        SPoint *points;
        SPoly(int iType, int n, double *x, double *y) :
            SRecord(iType), points(new SPoint[n]) {
            bounds.Set(lround(x[0]),lround(y[0]),lround(x[0]),lround(y[0]));
            count = n;
            for (int i = 0;  i < n;  ++i) {
                points[i].Set(lround(x[i]), lround(y[i]));
                if (points[i].x < bounds.left)   { bounds.left = points[i].x; }
                if (points[i].x > bounds.right)  { bounds.right = points[i].x; }
                if (points[i].y < bounds.bottom) { bounds.bottom = points[i].y;}
                if (points[i].y > bounds.top)    { bounds.top = points[i].y; }
            }
        }
        ~SPoly(void) { delete[] points; }
	void Serialize(std::string &o) const {
            o << iType << nSize << bounds << TUInt4(count);
            for (unsigned int i = 0;  i < count;  ++i) {
                o << points[i];
            }
	}
    };

    struct S_SETTEXTALIGN : SRecord {
        TUInt4 mode;
        S_SETTEXTALIGN(void) : SRecord(eEMR_SETTEXTALIGN) {}
	void Serialize(std::string &o) const {
            o << iType << nSize << mode;
        }
    };

    struct S_SETTEXTCOLOR : SRecord {
        SColorRef color;
        S_SETTEXTCOLOR(void) : SRecord(eEMR_SETTEXTCOLOR) {}
	void Serialize(std::string &o) const {
            o << iType << nSize << color;
        }
    };

    struct S_SELECTOBJECT : SRecord {
        TUInt4 ihObject;
        S_SELECTOBJECT(void) : SRecord(eEMR_SELECTOBJECT) {}
	void Serialize(std::string &o) const {
            o << iType << nSize << ihObject;
        }
    };

    struct S_SETBKMODE : SRecord {
        TUInt4 mode;
        S_SETBKMODE(void) : SRecord(eEMR_SETBKMODE) {}
	void Serialize(std::string &o) const {
            o << iType << nSize << mode;
        }
    };

    struct S_SETMAPMODE : SRecord {
        TUInt4 mode;
        S_SETMAPMODE(void) : SRecord(eEMR_SETMAPMODE) {}
	void Serialize(std::string &o) const {
            o << iType << nSize << mode;
        }
    };

    struct S_SETMITERLIMIT : SRecord {
        TUInt4 miterLimit;
        S_SETMITERLIMIT(void) : SRecord(eEMR_SETMITERLIMIT) {}
	void Serialize(std::string &o) const {
            o << iType << nSize << miterLimit;
        }
    };

    struct S_EOF : SRecord {
        TUInt4 nPalEntries;
        TUInt4 offPalEntries;
        TUInt4 nSizeLast;
        S_EOF(void) : SRecord(eEMR_EOF) {}
	void Serialize(std::string &o) const {
            o << iType << nSize << nPalEntries << offPalEntries << nSizeLast;
        }
    };

    struct S_RECTANGLE : SRecord {
        SRect box;
        S_RECTANGLE(void) : SRecord(eEMR_RECTANGLE) {}
	void Serialize(std::string &o) const {
            o << iType << nSize << box;
        }
    };

    struct S_ELLIPSE : SRecord {
        SRect box;
        S_ELLIPSE(void) : SRecord(eEMR_ELLIPSE) {}
	void Serialize(std::string &o) const {
            o << iType << nSize << box;
        }
    };

} //end of EMF namespace
