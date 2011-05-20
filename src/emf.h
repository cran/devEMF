/* $Id: emf.h 175 2011-05-20 15:49:32Z pjohnson $
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
        EMR_HEADER = 1,
        EMR_POLYGON = 3,
        EMR_POLYLINE = 4,
        EMR_EOF = 14,
        EMR_SETMAPMODE = 17,
        EMR_SETBKMODE = 18,
        EMR_SETTEXTALIGN = 22,
        EMR_SETTEXTCOLOR = 24,
        EMR_SELECTOBJECT = 37,
        EMR_CREATEPEN = 38,
        EMR_CREATEBRUSHINDIRECT = 39,
        EMR_ELLIPSE = 42,
        EMR_RECTANGLE = 43,
        EMR_EXTCREATEFONTINDIRECTW = 82,
        EMR_EXTTEXTOUTW = 84,
        EMR_EXTCREATEPEN = 95
    };

    enum EPenStyle {
        PS_SOLID       =  0x00000000,
        PS_DASH        =  0x00000001,
        PS_DOT         =  0x00000002,
        PS_DASHDOT     =  0x00000003,
        PS_DASHDOTDOT  =  0x00000004,
        PS_NULL        =  0x00000005,
        PS_INSIDEFRAME =  0x00000006,
        PS_USERSTYLE   =  0x00000007,
        PS_ALTERNATE   =  0x00000008,
        PS_STYLE_MASK  =  0x0000000f,
        PS_GEOMETRIC   =  0x00010000
    };

    enum EBrushStyle {
        BS_SOLID = 0,
        BS_NULL  = 1
    };
    
    enum EFontCharset {
        DEFAULT_CHARSET = 1,
        SYMBOL_CHARSET  = 2
    };

    enum EFontOutPrecision {
        OUT_DEFAULT_PRECIS   = 0,
        OUT_STRING_PRECIS    = 1,
        OUT_CHARACTER_PRECIS = 2,
        OUT_STROKE_PRECIS    = 3
    };

    enum EFontClipPrecision {
        CLIP_DEFAULT_PRECIS    = 0x00,
        CLIP_CHARACTER_PRECIS  = 0x01,
        CLIP_STROKE_PRECIS     = 0x02
    };

    enum EFontQuality {
        DEFAULT_QUALITY       = 0,
        DRAFT_QUALITY         = 1,
        PROOF_QUALITY         = 2,
        NONANTIALIASED_QUALITY= 3,
        ANTIALIASED_QUALITY   = 4,
        CLEARTYPE_QUALITY     = 5
    };

    enum EFontPitchFamily {
        DEFAULT_PITCH      = 0x00,
        FIXED_PITCH        = 0x01,
        VARIABLE_PITCH     = 0x02,

        FF_DONTCARE        = 0x00,
        FF_ROMAN           = 0x10,
        FF_SWISS           = 0x20,
        FF_MODERN          = 0x30,
        FF_SCRIPT          = 0x40,
        FF_DECORATIVE      = 0x50
    };

    enum ETextAlign {
        TA_LEFT            = 0x00,
        TA_TOP             = 0x00,
        TA_RIGHT           = 0x02,
        TA_CENTER          = 0x06,
        TA_BOTTOM          = 0x08
    };

    enum EBkMode {
        TRANSPARENT      = 1,
        OPAQUE           = 2
    };

    enum EMapMode {
        MM_TEXT		 = 1,
        MM_LOMETRIC	 = 2,
        MM_HIMETRIC	 = 3,
        MM_LOENGLISH	 = 4,
        MM_HIENGLISH	 = 5,
        MM_TWIPS	 = 6,
        MM_ISOTROPIC	 = 7,
        MM_ANISOTROPIC	 = 8
    };

    enum EGraphicsMode {
        GM_COMPATIBLE    = 1,
        GM_ADVANCED      = 2
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
        SRecord(unsigned int t) : iType(t) {}
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
        SHeader(void) : SRecord(EMR_HEADER) {}
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
        S_EXTTEXTOUTW(void) : SRecord(EMR_EXTTEXTOUTW) {}
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
        S_EXTCREATEPEN(void) : SRecord(EMR_EXTCREATEPEN), styleEntries(NULL) {}
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
	SBrush(void) : SRecord(EMR_CREATEBRUSHINDIRECT) {}
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
	SFont(void) : SRecord(EMR_EXTCREATEFONTINDIRECTW) {
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
            bounds.Set(x[0],y[0],x[0],y[0]);
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
        S_SETTEXTALIGN(void) : SRecord(EMR_SETTEXTALIGN) {}
	void Serialize(std::string &o) const {
            o << iType << nSize << mode;
        }
    };

    struct S_SETTEXTCOLOR : SRecord {
        SColorRef color;
        S_SETTEXTCOLOR(void) : SRecord(EMR_SETTEXTCOLOR) {}
	void Serialize(std::string &o) const {
            o << iType << nSize << color;
        }
    };

    struct S_SELECTOBJECT : SRecord {
        TUInt4 ihObject;
        S_SELECTOBJECT(void) : SRecord(EMR_SELECTOBJECT) {}
	void Serialize(std::string &o) const {
            o << iType << nSize << ihObject;
        }
    };

    struct S_SETBKMODE : SRecord {
        TUInt4 mode;
        S_SETBKMODE(void) : SRecord(EMR_SETBKMODE) {}
	void Serialize(std::string &o) const {
            o << iType << nSize << mode;
        }
    };

    struct S_SETMAPMODE : SRecord {
        TUInt4 mode;
        S_SETMAPMODE(void) : SRecord(EMR_SETMAPMODE) {}
	void Serialize(std::string &o) const {
            o << iType << nSize << mode;
        }
    };

    struct S_EOF : SRecord {
        TUInt4 nPalEntries;
        TUInt4 offPalEntries;
        TUInt4 nSizeLast;
        S_EOF(void) : SRecord(EMR_EOF) {}
	void Serialize(std::string &o) const {
            o << iType << nSize << nPalEntries << offPalEntries << nSizeLast;
        }
    };

    struct S_RECTANGLE : SRecord {
        SRect box;
        S_RECTANGLE(void) : SRecord(EMR_RECTANGLE) {}
	void Serialize(std::string &o) const {
            o << iType << nSize << box;
        }
    };

    struct S_ELLIPSE : SRecord {
        SRect box;
        S_ELLIPSE(void) : SRecord(EMR_ELLIPSE) {}
	void Serialize(std::string &o) const {
            o << iType << nSize << box;
        }
    };

} //end of EMF namespace
