/*
 * Most code in the file is extracted verbatim from devPS.c from
 * R-2.12.1 for dealing with font metrics.  Slight #if modifications
 * to make handling of gzip i/o compatible with R-2.14.0.  Only
 * additions are a few #defines at the very beginning of the file.
 * Only changes are replacing all occurences of "(_(" with "((".
 *
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 1995, 1996  Robert Gentleman and Ross Ihaka
 *  Copyright (C) 1998--2010  Robert Gentleman, Ross Ihaka and the
 *                            R Development Core Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, a copy is available at
 *  http://www.r-project.org/Licenses/
 */

//Added defines/declarations copied from Defn.h
#define streql(s, t)	(!strcmp((s), (t)))
# define FILESEP     "/"
LibExtern char *R_Home;             /* Root of the R tree */
//end of copied defines
//Added defines to substitute for missing functions
#define R_fopen fopen
#define warning Rf_warning
#define error Rf_error
#define isString		Rf_isString
#define asLogical		Rf_asLogical
#define ScalarLogical		Rf_ScalarLogical
#define ScalarString		Rf_ScalarString
#define mkChar			Rf_mkChar
#define install			Rf_install
#define findVar			Rf_findVar
#define eval			Rf_eval
#define getAttrib		Rf_getAttrib
//end of defines

#include <ctype.h>
#include <errno.h>

#include <Rversion.h>
#if R_VERSION >= 134656 //revisions in 2.14.0
  #include "zlib.h"
  /* from connections.o */
  extern "C" gzFile R_gzopen (const char *path, const char *mode);
  extern "C" char *R_gzgets(gzFile file, char *buf, int len);
  extern "C" int R_gzclose (gzFile file);
#else
#ifdef WIN32
# define USE_GZIO
#endif
#endif

#ifdef USE_GZIO
#include <zlib.h>
#endif

#include <R.h>
#include <Rinternals.h>

#define R_USE_PROTOTYPES 1
#include <R_ext/GraphicsEngine.h>
#include <R_ext/Error.h>
#include <R_ext/RS.h>

/* Part 1.  AFM File Parsing.  */

/* These are the basic entities in the AFM file */

#define BUFSIZE 512
#define NA_SHORT -30000

typedef struct {
    unsigned char c1;
    unsigned char c2;
    short kern;
} KP;

typedef struct {
    short FontBBox[4];
    short CapHeight;
    short XHeight;
    short Descender;
    short Ascender;
    short StemH;
    short StemV;
    short ItalicAngle;
    struct {
	short WX;
	short BBox[4];
    } CharInfo[256];
    KP *KernPairs;
    short KPstart[256];
    short KPend[256];
    short nKP;
    short IsFixedPitch;
} FontMetricInfo;

enum {
    Empty,
    StartFontMetrics,
    Comment,
    FontName,
    EncodingScheme,
    FullName,
    FamilyName,
    Weight,
    ItalicAngle,
    IsFixedPitch,
    UnderlinePosition,
    UnderlineThickness,
    Version,
    Notice,
    FontBBox,
    CapHeight,
    XHeight,
    Descender,
    Ascender,
    StartCharMetrics,
    C,
    CH,
    EndCharMetrics,
    StartKernData,
    StartKernPairs,
    KPX,
    EndKernPairs,
    EndKernData,
    StartComposites,
    CC,
    EndComposites,
    EndFontMetrics,
    StdHW,
    StdVW,
    CharacterSet,
    Unknown
};

static const struct {
    const char *keyword;
    const int code;
}
KeyWordDictionary[] = {
    { "StartFontMetrics",    StartFontMetrics },
    { "Comment",	     Comment },
    { "FontName",	     FontName },
    { "EncodingScheme",	     EncodingScheme },
    { "FullName",	     FullName },
    { "FamilyName",	     FamilyName },
    { "Weight",		     Weight },
    { "ItalicAngle",	     ItalicAngle },
    { "IsFixedPitch",	     IsFixedPitch },
    { "UnderlinePosition",   UnderlinePosition },
    { "UnderlineThickness",  UnderlineThickness },
    { "Version",	     Version },
    { "Notice",		     Notice },
    { "FontBBox",	     FontBBox },
    { "CapHeight",	     CapHeight },
    { "XHeight",	     XHeight },
    { "Descender",	     Descender },
    { "Ascender",	     Ascender },
    { "StartCharMetrics",    StartCharMetrics },
    { "C ",		     C },
    { "CH ",		     CH },
    { "EndCharMetrics",	     EndCharMetrics },
    { "StartKernData",	     StartKernData },
    { "StartKernPairs",	     StartKernPairs },
    { "KPX ",		     KPX },
    { "EndKernPairs",	     EndKernPairs },
    { "EndKernData",	     EndKernData },
    { "StartComposites",     StartComposites },
    { "CC ",		     CC },
    { "EndComposites",	     EndComposites },
    { "EndFontMetrics",	     EndFontMetrics },
    { "StdHW",		     StdHW },
    { "StdVW",		     StdVW },
    { "CharacterSet",	     CharacterSet},
    { NULL,		     Unknown },
};

static int MatchKey(char const * l, char const * k)
{
    while (*k)
	if (*k++ != *l++) return 0;
    return 1;
}

static int KeyType(const char * const s)
{
    int i;
    if (*s == '\n')
	return Empty;
    for (i = 0; KeyWordDictionary[i].keyword; i++)
	if (MatchKey(s, KeyWordDictionary[i].keyword))
	    return KeyWordDictionary[i].code;
    Rprintf("Unknown %s\n", s);
    return Unknown;
}

static char *SkipToNextItem(char *p)
{
    while (!isspace((int)*p)) p++;
    while (isspace((int)*p)) p++;
    return p;
}

static char *SkipToNextKey(char *p)
{
    while (*p != ';') p++;
    p++;
    while (isspace((int)*p)) p++;
    return p;
}

static int GetFontBBox(const char *buf, FontMetricInfo *metrics)
{
    if (sscanf(buf, "FontBBox %hd %hd %hd %hd",
	      &(metrics->FontBBox[0]),
	      &(metrics->FontBBox[1]),
	      &(metrics->FontBBox[2]),
	      &(metrics->FontBBox[3])) != 4) return 0;
#ifdef DEBUG_PS2
    Rprintf("FontBBox %d %d %d %d\n",
	    (metrics->FontBBox[0]),
	    (metrics->FontBBox[1]),
	    (metrics->FontBBox[2]),
	    (metrics->FontBBox[3]));
#endif
    return 1;
}

/* The longest named Adobe glyph is 39 chars:
   whitediamondcontainingblacksmalldiamond
 */
typedef struct {
    char cname[40];
} CNAME;


/* If reencode > 0, remap to new encoding */
static int GetCharInfo(char *buf, FontMetricInfo *metrics,
		       CNAME *charnames, CNAME *encnames,
		       int reencode)
{
    char *p = buf, charname[40];
    int nchar, nchar2 = -1, i;
    short WX;

    if (!MatchKey(buf, "C ")) return 0;
    p = SkipToNextItem(p);
    sscanf(p, "%d", &nchar);
    if ((nchar < 0 || nchar > 255) && !reencode) return 1;
    p = SkipToNextKey(p);

    if (!MatchKey(p, "WX")) return 0;
    p = SkipToNextItem(p);
    sscanf(p, "%hd", &WX);
    p = SkipToNextKey(p);

    if (!MatchKey(p, "N ")) return 0;
    p = SkipToNextItem(p);
    if(reencode) {
	sscanf(p, "%s", charname);
#ifdef DEBUG_PS2
	Rprintf("char name %s\n", charname);
#endif
	/* a few chars appear twice in ISOLatin1 */
	nchar = nchar2 = -1;
	for (i = 0; i < 256; i++)
	    if(!strcmp(charname, encnames[i].cname)) {
		strcpy(charnames[i].cname, charname);
		if(nchar == -1) nchar = i; else nchar2 = i;
	    }
	if (nchar == -1) return 1;
    } else {
	sscanf(p, "%s", charnames[nchar].cname);
    }
    metrics->CharInfo[nchar].WX = WX;
    p = SkipToNextKey(p);

    if (!MatchKey(p, "B ")) return 0;
    p = SkipToNextItem(p);
    sscanf(p, "%hd %hd %hd %hd",
	   &(metrics->CharInfo[nchar].BBox[0]),
	   &(metrics->CharInfo[nchar].BBox[1]),
	   &(metrics->CharInfo[nchar].BBox[2]),
	   &(metrics->CharInfo[nchar].BBox[3]));

#ifdef DEBUG_PS2
    Rprintf("nchar = %d %d %d %d %d %d\n", nchar,
	    metrics->CharInfo[nchar].WX,
	    metrics->CharInfo[nchar].BBox[0],
	    metrics->CharInfo[nchar].BBox[1],
	    metrics->CharInfo[nchar].BBox[2],
	    metrics->CharInfo[nchar].BBox[3]);
#endif
    if (nchar2 > 0) {
	metrics->CharInfo[nchar2].WX = WX;
	sscanf(p, "%hd %hd %hd %hd",
	       &(metrics->CharInfo[nchar2].BBox[0]),
	       &(metrics->CharInfo[nchar2].BBox[1]),
	       &(metrics->CharInfo[nchar2].BBox[2]),
	       &(metrics->CharInfo[nchar2].BBox[3]));

#ifdef DEBUG_PS2
	Rprintf("nchar = %d %d %d %d %d %d\n", nchar2,
		metrics->CharInfo[nchar2].WX,
		metrics->CharInfo[nchar2].BBox[0],
		metrics->CharInfo[nchar2].BBox[1],
		metrics->CharInfo[nchar2].BBox[2],
		metrics->CharInfo[nchar2].BBox[3]);
#endif
    }
    return 1;
}

static int GetKPX(char *buf, int nkp, FontMetricInfo *metrics,
		  CNAME *charnames)
{
    char *p = buf, c1[50], c2[50];
    int i, done = 0;

    p = SkipToNextItem(p);
    sscanf(p, "%s %s %hd", c1, c2, &(metrics->KernPairs[nkp].kern));
    if (streql(c1, "space") || streql(c2, "space")) return 0;
    for(i = 0; i < 256; i++) {
	if (!strcmp(c1, charnames[i].cname)) {
	    metrics->KernPairs[nkp].c1 = i;
	    done++;
	    break;
	}
    }
    for(i = 0; i < 256; i++)
	if (!strcmp(c2, charnames[i].cname)) {
	    metrics->KernPairs[nkp].c2 = i;
	    done++;
	    break;
	}
    return (done==2);
}

/* Encode File Parsing.  */
/* Statics here are OK, as all the calls are in one initialization
   so no concurrency (until threads?) */

typedef struct {
  /* Probably can make buf and p0 local variables. Only p needs to be
     stored across calls. Need to investigate this more closely. */
  char buf[1000];
  char *p;
  char *p0;
} EncodingInputState;

/* read in the next encoding item, separated by white space. */
static int GetNextItem(FILE *fp, char *dest, int c, EncodingInputState *state)
{
    if (c < 0) state->p = NULL;
    while (1) {
	if (feof(fp)) { state->p = NULL; return 1; }
	if (!state->p || *state->p == '\n' || *state->p == '\0') {
	    state->p = fgets(state->buf, 1000, fp);
	}
	/* check for incomplete encoding file */
	if(!state->p) return 1;
	while (isspace((int)* state->p)) state->p++;
	if (*state->p == '\0' || *state->p == '%'|| *state->p == '\n') { state->p = NULL; continue; }
	state->p0 = state->p;
	while (!isspace((int)*state->p)) state->p++;
	if (*state->p != '\0') *state->p++ = '\0';
	if(c == 45) strcpy(dest, "/minus"); else strcpy(dest, state->p0);
	break;
    }
    return 0;
}

/*
 * Convert the encoding file name into a name to be used with iconv()
 * in mbcsToSbcs()
 *
 * FIXME:  Doesn't trim path/to/encfile (i.e., doesn't handle
 *         custom encoding file selected by user).
 *         Also assumes that encpath has ".enc" suffix supplied
 *         (not required by R interface)
 */

static int pathcmp(const char *encpath, const char *comparison) {
    char pathcopy[PATH_MAX];
    char *p1, *p2;
    strcpy(pathcopy, encpath);
    /*
     * Strip path/to/encfile/
     */
    p1 = &(pathcopy[0]);
    while ((p2 = strchr(p1, FILESEP[0]))) {
	p1 = p2 + sizeof(char);
    }
    /*
     * Strip suffix
     */
    p2 = (strchr(p1, '.'));
    if (p2)
	*p2 = '\0';
    return strcmp(p1, comparison);
}

static void seticonvName(const char *encpath, char *convname)
{
    /*
     * Default to "latin1"
     */
    char *p;
    strcpy(convname, "latin1");
    if(pathcmp(encpath, "ISOLatin1")==0)
	strcpy(convname, "latin1");
    else if(pathcmp(encpath, "ISOLatin2")==0)
	strcpy(convname, "latin2");
    else if(pathcmp(encpath, "ISOLatin7")==0)
	strcpy(convname, "latin7");
    else if(pathcmp(encpath, "ISOLatin9")==0)
	strcpy(convname, "latin-9");
    else if (pathcmp(encpath, "WinAnsi")==0)
	strcpy(convname, "CP1252");
    else {
	/*
	 * Last resort = trim .enc off encpath to produce convname
	 */
	strcpy(convname, encpath);
	p = strrchr(convname, '.');
	if(p) *p = '\0';
    }
}

/* Load encoding array from a file: defaults to the R_HOME/library/grDevices/afm directory */

/*
 * encpath gives the file to read from
 * encname is filled with the encoding name from the file
 * encconvname is filled with a "translation" of the encoding name into
 *             one that can be used with iconv()
 * encnames is filled with the character names from the file
 * enccode is filled with the raw source of the file
 */
static int
LoadEncoding(const char *encpath, char *encname,
	     char *encconvname, CNAME *encnames,
	     char *enccode, Rboolean isPDF)
{
    char buf[BUFSIZE];
    int i;
    FILE *fp;
    EncodingInputState state;
    state.p = state.p0 = NULL;

    seticonvName(encpath, encconvname);

    if(strchr(encpath, FILESEP[0])) strcpy(buf, encpath);
    else snprintf(buf, BUFSIZE,"%s%slibrary%sgrDevices%senc%s%s",
		  R_Home, FILESEP, FILESEP, FILESEP, FILESEP, encpath);
#ifdef DEBUG_PS
    Rprintf("encoding path is %s\n", buf);
#endif
    if (!(fp = R_fopen(R_ExpandFileName(buf), "r"))) {
	strcat(buf, ".enc");
	if (!(fp = R_fopen(R_ExpandFileName(buf), "r"))) return 0;
    }
    if (GetNextItem(fp, buf, -1, &state)) return 0; /* encoding name */
    strcpy(encname, buf+1);
    if (!isPDF) snprintf(enccode, 5000, "/%s [\n", encname);
    else enccode[0] = '\0';
    if (GetNextItem(fp, buf, 0, &state)) { fclose(fp); return 0;} /* [ */
    for(i = 0; i < 256; i++) {
	if (GetNextItem(fp, buf, i, &state)) { fclose(fp); return 0; }
	strcpy(encnames[i].cname, buf+1);
	strcat(enccode, " /"); strcat(enccode, encnames[i].cname);
	if(i%8 == 7) strcat(enccode, "\n");
    }
    if (GetNextItem(fp, buf, 0, &state)) { fclose(fp); return 0;} /* ] */
    fclose(fp);
    if (!isPDF) strcat(enccode,"]\n");
    return 1;
}

/* Load font metrics from a file: defaults to the
   R_HOME/library/grDevices/afm directory */
static int
PostScriptLoadFontMetrics(const char * const fontpath,
			  FontMetricInfo *metrics,
			  char *fontname,
			  CNAME *charnames,
			  CNAME *encnames,
			  int reencode)
{
    char buf[BUFSIZE], *p, truth[10];
    int mode, i = 0, j, ii, nKPX=0;
#if R_VERSION >= 134656 //revisions in 2.14.0
    gzFile fp;
#else
#ifdef USE_GZIO
    gzFile fp;
#else
    FILE *fp;
#endif
#endif

    if(strchr(fontpath, FILESEP[0])) strcpy(buf, fontpath);
    else
#if R_VERSION >= 134656 //revisions in 2.14.0
	snprintf(buf, BUFSIZE,"%s%slibrary%sgrDevices%safm%s%s.gz",
		 R_Home, FILESEP, FILESEP, FILESEP, FILESEP, fontpath);
#else
#ifdef USE_GZIO
	snprintf(buf, BUFSIZE,"%s%slibrary%sgrDevices%safm%s%s.gz",
		 R_Home, FILESEP, FILESEP, FILESEP, FILESEP, fontpath);
#else
	snprintf(buf, BUFSIZE,"%s%slibrary%sgrDevices%safm%s%s",
		 R_Home, FILESEP, FILESEP, FILESEP, FILESEP, fontpath);
#endif
#endif
#ifdef DEBUG_PS
    Rprintf("afmpath is %s\n", buf);
    Rprintf("reencode is %d\n", reencode);
#endif

#if R_VERSION >= 134656 //revisions in 2.14.0
    if (!(fp = R_gzopen(R_ExpandFileName(buf), "rb"))) {
        /* try uncompressed version */
        snprintf(buf, BUFSIZE,"%s%slibrary%sgrDevices%safm%s%s",
                 R_Home, FILESEP, FILESEP, FILESEP, FILESEP, fontpath);
        if (!(fp = R_gzopen(R_ExpandFileName(buf), "rb"))) {
            warning("afm file '%s' could not be opened",
                    R_ExpandFileName(buf));
            return 0;
        }
    }
#else
#ifdef USE_GZIO
    if (!(fp = gzopen(R_ExpandFileName(buf), "rb"))) {
#else
    if (!(fp = R_fopen(R_ExpandFileName(buf), "r"))) {
#endif
	warning("afm file '%s' could not be opened",
		R_ExpandFileName(buf));
	return 0;
    }
#endif

    metrics->KernPairs = NULL;
    metrics->CapHeight = metrics->XHeight = metrics->Descender =
	metrics->Ascender = metrics->StemH = metrics->StemV = NA_SHORT;
    metrics->IsFixedPitch = -1;
    metrics->ItalicAngle = 0;
    mode = 0;
    for (ii = 0; ii < 256; ii++) {
	charnames[ii].cname[0] = '\0';
	metrics->CharInfo[ii].WX = NA_SHORT;
	for(j = 0; j < 4; j++) metrics->CharInfo[ii].BBox[j] = 0;
    }
#if R_VERSION >= 134656 //revisions in 2.14.0
    while (R_gzgets(fp, buf, BUFSIZE)) {
#else
#ifdef USE_GZIO
    while (gzgets(fp, buf, BUFSIZE)) {
#else
    while (fgets(buf, BUFSIZE, fp)) {
#endif
#endif
	switch(KeyType(buf)) {

	case StartFontMetrics:
	    mode = StartFontMetrics;
	    break;

	case EndFontMetrics:
	    mode = 0;
	    break;

	case FontBBox:
	    if (!GetFontBBox(buf, metrics)) {
		warning("FontBBox could not be parsed");
		goto pserror;
	    }
	    break;

	case C:
	    if (mode != StartFontMetrics) goto pserror;
	    if (!GetCharInfo(buf, metrics, charnames, encnames, reencode)) {
		warning("CharInfo could not be parsed");
		goto pserror;
	    }
	    break;

	case StartKernData:
	    mode = StartKernData;
	    break;

	case StartKernPairs:
	    if(mode != StartKernData) goto pserror;
	    p = SkipToNextItem(buf);
	    sscanf(p, "%d", &nKPX);
	    if(nKPX > 0) {
		/* nPKX == 0 should not happen, but has */
		metrics->KernPairs = (KP *) malloc(nKPX * sizeof(KP));
		if (!metrics->KernPairs) goto pserror;
	    }
	    break;

	case KPX:
	    if(mode != StartKernData || i >= nKPX) goto pserror;
	    if (GetKPX(buf, i, metrics, charnames)) i++;
	    break;

	case EndKernData:
	    mode = 0;
	    break;

	case Unknown:
	    warning("unknown AFM entity encountered");
	    break;

	case FontName:
	    p = SkipToNextItem(buf);
	    sscanf(p, "%[^\n\f\r]", fontname);
	    break;

	case CapHeight:
	    p = SkipToNextItem(buf);
	    sscanf(p, "%hd", &metrics->CapHeight);
	    break;

	case XHeight:
	    p = SkipToNextItem(buf);
	    sscanf(p, "%hd", &metrics->XHeight);
	    break;

	case Ascender:
	    p = SkipToNextItem(buf);
	    sscanf(p, "%hd", &metrics->Ascender);
	    break;

	case Descender:
	    p = SkipToNextItem(buf);
	    sscanf(p, "%hd", &metrics->Descender);
	    break;

	case StdHW:
	    p = SkipToNextItem(buf);
	    sscanf(p, "%hd", &metrics->StemH);
	    break;

	case StdVW:
	    p = SkipToNextItem(buf);
	    sscanf(p, "%hd", &metrics->StemV);
	    break;

	case ItalicAngle:
	    p = SkipToNextItem(buf);
	    sscanf(p, "%hd", &metrics->ItalicAngle);
	    break;

	case IsFixedPitch:
	    p = SkipToNextItem(buf);
	    sscanf(p, "%[^\n\f\r]", truth);
	    metrics->IsFixedPitch = strcmp(truth, "true") == 0;
	    break;

	case Empty:
	default:
	    break;
	}
    }
    metrics->nKP = i;
#if R_VERSION >= 134656 //revisions in 2.14.0
    R_gzclose(fp);
#else
#ifdef USE_GZIO
    gzclose(fp);
#else
    fclose(fp);
#endif
#endif
    /* Make an index for kern-pair searches: relies on having contiguous
       blocks by first char for efficiency, but works in all cases. */
    {
	short ind, tmp;
	for (j = 0; j < 256; j++) {
	    metrics->KPstart[j] = i;
	    metrics->KPend[j] = 0;
	}
	for (j = 0; j < i; j++) {
	    ind = metrics->KernPairs[j].c1;
	    tmp = metrics->KPstart[ind];
	    if(j < tmp) metrics->KPstart[ind] = j;
	    tmp = metrics->KPend[ind];
	    if(j > tmp) metrics->KPend[ind] = j;
	}
    }
    return 1;
pserror:
#if R_VERSION >= 134656 //revisions in 2.14.0
    R_gzclose(fp);
#else
#ifdef USE_GZIO
    gzclose(fp);
#else
    fclose(fp);
#endif
#endif
    return 0;
}

/* Be careful about the assumptions here.  In an 8-bit locale 0 <= c < 256
   and it is in the encoding in use.  As it is not going to be
   re-encoded when text is output, it is correct not to re-encode here.

   When called in an MBCS locale and font != 5, chars < 128 are sent
   as is (we assume that is ASCII) and others are re-encoded to
   Unicode in GEText (and interpreted as Unicode in GESymbol).
*/
# ifdef WORDS_BIGENDIAN
static const char UCS2ENC[] = "UCS-2BE";
# else
static const char UCS2ENC[] = "UCS-2LE";
# endif

static void
PostScriptMetricInfo(int c, double *ascent, double *descent, double *width,
		     FontMetricInfo *metrics,
		     Rboolean isSymbol,
		     const char *encoding)
{
    Rboolean Unicode = mbcslocale;

    if (c == 0) {
	*ascent = 0.001 * metrics->FontBBox[3];
	*descent = -0.001 * metrics->FontBBox[1];
	*width = 0.001 * (metrics->FontBBox[2] - metrics->FontBBox[0]);
	return;
    }

    if (c < 0) { Unicode = TRUE; c = -c; }
    /* We don't need the restriction to 65536 here any more as we could
       convert from  UCS4ENC, but there are few language chars above 65536. */
    if(Unicode && !isSymbol && c >= 128 && c < 65536) { /* Unicode */
	void *cd = NULL;
	const char *i_buf; char *o_buf, out[2];
	size_t i_len, o_len, status;
	unsigned short w[2];

	if ((void*)-1 == (cd = Riconv_open(encoding, UCS2ENC)))
	    error(("unknown encoding '%s' in 'PostScriptMetricInfo'"),
		  encoding);

	/* Here we use terminated strings, but could use one char */
	w[0] = c; w[1] = 0;
	i_buf = (char *)w;
	i_len = 4;
	o_buf = out;
	o_len = 2;
	status = Riconv(cd, &i_buf, (size_t *)&i_len,
			(char **)&o_buf, (size_t *)&o_len);
	Riconv_close(cd);
	if (status == (size_t)-1) {
	    *ascent = 0;
	    *descent = 0;
	    *width = 0;
	    warning(("font metrics unknown for Unicode character U+%04x"), c);
	    return;
	} else {
	    c = out[0] & 0xff;
	}
    }

    if (c > 255) { /* Unicode */
	*ascent = 0;
	*descent = 0;
	*width = 0;
	warning(("font metrics unknown for Unicode character U+%04x"), c);
    } else {
	short wx;

	*ascent = 0.001 * metrics->CharInfo[c].BBox[3];
	*descent = -0.001 * metrics->CharInfo[c].BBox[1];
	wx = metrics->CharInfo[c].WX;
	if(wx == NA_SHORT) {
	    warning(("font metrics unknown for character 0x%x"), c);
	    wx = 0;
	}
	*width = 0.001 * wx;
    }
}

/*******************************************************
 * Data structures and functions for loading Type 1 fonts into an R session.
 *
 * Used by PostScript, XFig and PDF drivers.
 *
 * The idea is that font information is only loaded once for each font
 * within an R session.  Also, each encoding is only loaded once per
 * session.  A global list of loaded fonts and a global list of
 * loaded encodings are maintained.  Devices maintain their own list
 * of fonts and encodings used on the device;  the elements of these
 * lists are just pointers to the elements of the global lists.
 *
 * Cleaning up device lists just involves free'ing the lists themselves.
 * When the R session closes, the actual font and encoding information
 * is unloaded using the global lists.
 */

/*
 * Information about one Type 1 font
 */
typedef struct CIDFontInfo {
    char name[50];
} CIDFontInfo, *cidfontinfo;

typedef struct T1FontInfo {
    char name[50];
    FontMetricInfo metrics;
    CNAME charnames[256];
} Type1FontInfo, *type1fontinfo;

/*
 * Information about a font encoding
 */
typedef struct EncInfo {
    char encpath[PATH_MAX];
    char name[100]; /* Name written to PostScript/PDF file */
    char convname[50]; /* Name used in mbcsToSbcs() with iconv() */
    CNAME encnames[256];
    char enccode[5000];
} EncodingInfo, *encodinginfo;

/*
 * Information about a font family
 * (5 fonts representing plain, bold, italic, bolditalic, and symbol)
 *
 * The name is a graphics engine font family name
 * (distinct from the Type 1 font name)
 */
typedef struct CIDFontFamily {
    char fxname[50];
    cidfontinfo cidfonts[4];
    type1fontinfo symfont;
    char cmap[50];
    char encoding[50];
} CIDFontFamily, *cidfontfamily;

typedef struct T1FontFamily {
    char fxname[50];
    type1fontinfo fonts[5];
    encodinginfo encoding;
} Type1FontFamily, *type1fontfamily;

/*
 * A list of Type 1 font families
 *
 * Used to keep track of fonts currently loaded in the session
 * AND by each device to keep track of fonts currently used on the device.
 */
typedef struct CIDFontList {
    cidfontfamily cidfamily;
    struct CIDFontList *next;
} CIDFontList, *cidfontlist;

typedef struct T1FontList {
    type1fontfamily family;
    struct T1FontList *next;
} Type1FontList, *type1fontlist;

/*
 * Same as type 1 font list, but for encodings.
 */
typedef struct EncList {
    encodinginfo encoding;
    struct EncList *next;
} EncodingList, *encodinglist;

/*
 * Various constructors and destructors
 */

static type1fontinfo makeType1Font()
{
    type1fontinfo font = (Type1FontInfo *) malloc(sizeof(Type1FontInfo));
    /*
     * Initialise font->metrics.KernPairs to NULL
     * so that we know NOT to free it if we fail to
     * load this font and have to
     * bail out and free this type1fontinfo
     */
    font->metrics.KernPairs = NULL;
    if (!font)
	warning(("failed to allocate Type 1 font info"));
    return font;
}

static void freeType1Font(type1fontinfo font)
{
    if (font->metrics.KernPairs)
	free(font->metrics.KernPairs);
    free(font);
}

static encodinginfo makeEncoding()
{
    encodinginfo encoding = (EncodingInfo *) malloc(sizeof(EncodingInfo));
    if (!encoding)
	warning(("failed to allocate encoding info"));
    return encoding;
}

static void freeEncoding(encodinginfo encoding)
{
    free(encoding);
}

static type1fontfamily makeFontFamily()
{
    type1fontfamily family = (Type1FontFamily *) malloc(sizeof(Type1FontFamily));
    if (family) {
	int i;
	for (i = 0; i < 5; i++)
	    family->fonts[i] = NULL;
	family->encoding = NULL;
    } else
	warning(("failed to allocate Type 1 font family"));
    return family;
}
/*
 * Frees a font family, including fonts, but NOT encoding
 *
 * Used by global font list to free all fonts loaded in session
 * (should not be used by devices; else may free fonts more than once)
 *
 * Encodings are freed using the global encoding list
 * (to ensure that each encoding is only freed once)
 */

static void freeFontFamily(type1fontfamily family)
{
    int i;
    for (i=0; i<5; i++)
	if (family->fonts[i])
	    freeType1Font(family->fonts[i]);
    free(family);
}


static type1fontlist makeFontList()
{
    type1fontlist fontlist = (Type1FontList *) malloc(sizeof(Type1FontList));
    if (fontlist) {
	fontlist->family = NULL;
	fontlist->next = NULL;
    } else
	warning(("failed to allocate font list"));
    return fontlist;
}

/*
 * Just free the Type1FontList structure, do NOT free elements it points to
 *
 * Used by both global font list and devices to free the font lists
 * (global font list separately takes care of the fonts pointed to)
 */
static void freeCIDFontList(cidfontlist fontlist) {
    /*
     * These will help to find any errors if attempt to
     * use freed font list.
     */
    fontlist->cidfamily = NULL;
    fontlist->next = NULL;
    free(fontlist);
}
static void freeFontList(type1fontlist fontlist) {
    /*
     * These will help to find any errors if attempt to
     * use freed font list.
     */
    fontlist->family = NULL;
    fontlist->next = NULL;
    free(fontlist);
}

static void freeDeviceCIDFontList(cidfontlist fontlist) {
    if (fontlist) {
	if (fontlist->next)
	    freeDeviceCIDFontList(fontlist->next);
	freeCIDFontList(fontlist);
    }
}
static void freeDeviceFontList(type1fontlist fontlist) {
    if (fontlist) {
	if (fontlist->next)
	    freeDeviceFontList(fontlist->next);
	freeFontList(fontlist);
    }
}

static encodinglist makeEncList()
{
    encodinglist enclist = (EncodingList *) malloc(sizeof(EncodingList));
    if (enclist) {
	enclist->encoding = NULL;
	enclist->next = NULL;
    } else
	warning(("failed to allocated encoding list"));
    return enclist;
}

static void freeEncList(encodinglist enclist)
{
    enclist->encoding = NULL;
    enclist->next = NULL;
    free(enclist);
}

static void freeDeviceEncList(encodinglist enclist) {
    if (enclist) {
	if (enclist->next)
	    freeDeviceEncList(enclist->next);
	freeEncList(enclist);
    }
}

/*
 * Global list of fonts and encodings that have been loaded this session
 */
static cidfontlist loadedCIDFonts = NULL;
static type1fontlist loadedFonts = NULL;
static encodinglist loadedEncodings = NULL;
/*
 * There are separate PostScript and PDF font databases at R level
 * so MUST have separate C level records too
 * (because SAME device-independent font family name could map
 *  to DIFFERENT font for PostScript and PDF)
 */
static cidfontlist PDFloadedCIDFonts = NULL;
static type1fontlist PDFloadedFonts = NULL;
static encodinglist PDFloadedEncodings = NULL;

/*
 * Names of R level font databases
 */
static char PostScriptFonts[] = ".PostScript.Fonts";
static char PDFFonts[] = ".PDF.Fonts";

/*
 * Free the above globals
 *
 * NOTE that freeing the font families does NOT free the encodings
 * Hence we free all encodings first.
 */

/* NB this is exported, and was at some point used by KillAllDevices
   in src/main/graphics.c.  That would be a problem now it is in a
   separate DLL.
*/
#if 0
void freeType1Fonts()
{
    encodinglist enclist = loadedEncodings;
    type1fontlist fl = loadedFonts;
    cidfontlist   cidfl = loadedCIDFonts;
    type1fontlist pdffl = PDFloadedFonts;
    cidfontlist   pdfcidfl = PDFloadedCIDFonts;
    while (enclist) {
	enclist = enclist->next;
	freeEncoding(loadedEncodings->encoding);
	freeEncList(loadedEncodings);
	loadedEncodings = enclist;
    }
    while (fl) {
	fl = fl->next;
	freeFontFamily(loadedFonts->family);
	freeFontList(loadedFonts);
	loadedFonts = fl;
    }
    while (cidfl) {
	cidfl = cidfl->next;
	freeCIDFontFamily(loadedCIDFonts->cidfamily);
	freeCIDFontList(loadedCIDFonts);
	loadedCIDFonts = cidfl;
    }
    while (pdffl) {
	pdffl = pdffl->next;
	freeFontFamily(PDFloadedFonts->family);
	freeFontList(PDFloadedFonts);
	PDFloadedFonts = pdffl;
    }
    while (pdfcidfl) {
	pdfcidfl = pdfcidfl->next;
	freeCIDFontFamily(PDFloadedCIDFonts->cidfamily);
	freeCIDFontList(PDFloadedCIDFonts);
	PDFloadedCIDFonts = pdfcidfl;
    }
}
#endif

/*
 * Given a path to an encoding file,
 * find an EncodingInfo that corresponds
 */
static encodinginfo
findEncoding(const char *encpath, encodinglist deviceEncodings, Rboolean isPDF)
{
    encodinglist enclist = isPDF ? PDFloadedEncodings : loadedEncodings;
    encodinginfo encoding = NULL;
    int found = 0;
    /*
     * "default" is a special encoding which means use the
     * default (FIRST) encoding set up ON THIS DEVICE.
     */
    if (!strcmp(encpath, "default")) {
	found = 1;
	encoding = deviceEncodings->encoding;
    } else {
	while (enclist && !found) {
	    found = !strcmp(encpath, enclist->encoding->encpath);
	    if (found)
		encoding = enclist->encoding;
	    enclist = enclist->next;
	}
    }
    return encoding;
}

/*
 * Utility to avoid string overrun
 */
static void safestrcpy(char *dest, const char *src, int maxlen)
{
    if ((int) strlen(src) < maxlen)
	strcpy(dest, src);
    else {
	warning(("truncated string which was too long for copy"));
	strncpy(dest, src, maxlen-1);
	dest[maxlen-1] = '\0';
    }
}

/*
 * Add an encoding to the list of loaded encodings ...
 *
 * ... and return the new encoding
 */
static encodinginfo addEncoding(const char *encpath, Rboolean isPDF)
{
    encodinginfo encoding = makeEncoding();
    if (encoding) {
	if (LoadEncoding(encpath,
			 encoding->name,
			 encoding->convname,
			 encoding->encnames,
			 encoding->enccode,
			 isPDF)) {
	    encodinglist newenc = makeEncList();
	    if (!newenc) {
		freeEncoding(encoding);
		encoding = NULL;
	    } else {
		encodinglist enclist =
		    isPDF ? PDFloadedEncodings : loadedEncodings;
		safestrcpy(encoding->encpath, encpath, PATH_MAX);
		newenc->encoding = encoding;
		if (!enclist) {
		    if(isPDF) PDFloadedEncodings = newenc;
		    else loadedEncodings = newenc;
		} else {
		    while (enclist->next)
			enclist = enclist->next;
		    enclist->next = newenc;
		}
	    }
	} else {
	    warning(("failed to load encoding file '%s'"), encpath);
	    freeEncoding(encoding);
	    encoding = NULL;
	}
    } else
	encoding = NULL;
    return encoding;
}

/*
 * Given a graphics engine font family name,
 * find a Type1FontFamily that corresponds
 *
 * If get fxname match, check whether the encoding in the
 * R database is "default"
 * (i.e., the graphics engine font family encoding is unspecified)
 * If it is "default" then check that the loaded encoding is the
 * same as the encoding we want.  A matching encoding is defined
 * as one which leads to the same iconvname (see seticonvName()).
 * This could perhaps be made more rigorous by actually looking inside
 * the relevant encoding file for the encoding name.
 *
 * If the encoding we want is NULL, then we just don't care.
 *
 * Returns NULL if can't find font in loadedFonts
 */

static const char *getFontEncoding(const char *family, const char *fontdbname);

static type1fontfamily
findLoadedFont(const char *name, const char *encoding, Rboolean isPDF)
{
    type1fontlist fontlist;
    type1fontfamily font = NULL;
    char *fontdbname;
    int found = 0;

    if (isPDF) {
	fontlist = PDFloadedFonts;
	fontdbname = PDFFonts;
    } else {
	fontlist = loadedFonts;
	fontdbname = PostScriptFonts;
    }
    while (fontlist && !found) {
	found = !strcmp(name, fontlist->family->fxname);
	if (found) {
	    font = fontlist->family;
	    if (encoding) {
		char encconvname[50];
		const char *encname = getFontEncoding(name, fontdbname);
		seticonvName(encoding, encconvname);
		if (!strcmp(encname, "default") &&
		    strcmp(fontlist->family->encoding->convname,
			   encconvname)) {
		    font = NULL;
		    found = 0;
		}
	    }
	}
	fontlist = fontlist->next;
    }
    return font;
}

SEXP Type1FontInUse(SEXP name, SEXP isPDF)
{
    if (!isString(name) || LENGTH(name) > 1)
	error(("Invalid font name or more than one font name"));
    return ScalarLogical(
                         findLoadedFont(CHAR(STRING_ELT(name, 0)), NULL, (Rboolean)asLogical(isPDF))
	!= NULL);
}

static cidfontfamily findLoadedCIDFont(const char *family, Rboolean isPDF)
{
    cidfontlist fontlist;
    cidfontfamily font = NULL;
    int found = 0;

    if (isPDF) {
	fontlist = PDFloadedCIDFonts;
    } else {
	fontlist = loadedCIDFonts;
    }
    while (fontlist && !found) {
	found = !strcmp(family, fontlist->cidfamily->cidfonts[0]->name);
	if (found)
	    font = fontlist->cidfamily;
	fontlist = fontlist->next;
    }
#ifdef PS_DEBUG
    if(found)
	Rprintf("findLoadedCIDFont found = %s\n",family);
#endif
    return font;
}

SEXP CIDFontInUse(SEXP name, SEXP isPDF)
{
    if (!isString(name) || LENGTH(name) > 1)
	error(("Invalid font name or more than one font name"));
    return ScalarLogical(
                         findLoadedCIDFont(CHAR(STRING_ELT(name, 0)), (Rboolean)asLogical(isPDF))
	!= NULL);
}

/*
 * Must only be called once a device has at least one font added
 * (i.e., after the default font has been added)
 */
static type1fontfamily
findDeviceFont(const char *name, type1fontlist fontlist, int *index)
{
    type1fontfamily font = NULL;
    int found = 0;
    *index = 0;
    /*
     * If the graphics engine font family is ""
     * just use the default font that was loaded when the device
     * was created.
     * This will (MUST) be the first font in the device
     */
    if (strlen(name) > 0) {
	while (fontlist && !found) {
	    found = !strcmp(name, fontlist->family->fxname);
	    if (found)
		font = fontlist->family;
	    fontlist = fontlist->next;
	    *index = *index + 1;
	}
    } else {
	font = fontlist->family;
	*index = 1;
    }
    return font;
}

/*
 * Get an R-level font database
 */
static SEXP getFontDB(const char *fontdbname) {
    SEXP graphicsNS, PSenv;
    SEXP fontdb;
    PROTECT(graphicsNS = R_FindNamespace(ScalarString(mkChar("grDevices"))));
    PROTECT(PSenv = findVar(install(".PSenv"), graphicsNS));
    /* under lazy loading this will be a promise on first use */
    if(TYPEOF(PSenv) == PROMSXP) {
	PROTECT(PSenv);
	PSenv = eval(PSenv, graphicsNS);
	UNPROTECT(1);
    }
    PROTECT(fontdb = findVar(install(fontdbname), PSenv));
    UNPROTECT(3);
    return fontdb;
}

/*
 * Get the path to the afm file for a user-specifed font
 * given a graphics engine font family and the face
 * index (0..4)
 *
 * Do this by looking up the font name in the PostScript
 * font database
 */
static const char*
fontMetricsFileName(const char *family, int faceIndex,
		    const char *fontdbname)
{
    int i, nfonts;
    const char *result = NULL;
    int found = 0;
    SEXP fontdb = getFontDB(fontdbname);
    SEXP fontnames;
    PROTECT(fontnames = getAttrib(fontdb, R_NamesSymbol));
    nfonts = LENGTH(fontdb);
    for (i = 0; i < nfonts && !found; i++) {
	const char *fontFamily = CHAR(STRING_ELT(fontnames, i));
	if (strcmp(family, fontFamily) == 0) {
	    found = 1;
	    /* 1 means vector of font afm file paths */
	    result = CHAR(STRING_ELT(VECTOR_ELT(VECTOR_ELT(fontdb, i), 1),
				     faceIndex));
	}
    }
    if (!found)
	warning(("font family not found in PostScript font database"));
    UNPROTECT(1);
    return result;
}


/*
 * Get encoding name from font database
 */
static const char *getFontEncoding(const char *family, const char *fontdbname)
{
    SEXP fontnames;
    int i, nfonts;
    const char *result = NULL;
    int found = 0;
    SEXP fontdb = getFontDB(fontdbname);
    PROTECT(fontnames = getAttrib(fontdb, R_NamesSymbol));
    nfonts = LENGTH(fontdb);
    for (i=0; i<nfonts && !found; i++) {
	const char *fontFamily = CHAR(STRING_ELT(fontnames, i));
	if (strcmp(family, fontFamily) == 0) {
	    found = 1;
	    /* 2 means 'encoding' element */
	    result = CHAR(STRING_ELT(VECTOR_ELT(VECTOR_ELT(fontdb, i), 2), 0));
	}
    }
    if (!found)
	warning(("font encoding not found in font database"));
    UNPROTECT(1);
    return result;
}


static type1fontfamily addLoadedFont(type1fontfamily font,
				     Rboolean isPDF)
{
    type1fontlist newfont = makeFontList();
    if (!newfont) {
	freeFontFamily(font);
	font = NULL;
    } else {
	type1fontlist fontlist;
	if (isPDF)
	    fontlist = PDFloadedFonts;
	else
	    fontlist = loadedFonts;
	newfont->family = font;
	if (!fontlist) {
	    if (isPDF)
		PDFloadedFonts = newfont;
	    else
		loadedFonts = newfont;
	} else {
	    while (fontlist->next)
		fontlist = fontlist->next;
	    fontlist->next = newfont;
	}
    }
    return font;
}

static type1fontfamily addFont(const char *name, Rboolean isPDF,
			       encodinglist deviceEncodings)
{
    type1fontfamily fontfamily = makeFontFamily();
    char *fontdbname;
    if (isPDF)
	fontdbname = PDFFonts;
    else
	fontdbname = PostScriptFonts;
    if (fontfamily) {
	int i;
	encodinginfo encoding;
	const char *encpath = getFontEncoding(name, fontdbname);
	if (!encpath) {
	    freeFontFamily(fontfamily);
	    fontfamily = NULL;
	} else {
	    /*
	     * Set the name of the font
	     */
	    safestrcpy(fontfamily->fxname, name, 50);
	    /*
	     * Find or add encoding
	     */
	    if (!(encoding = findEncoding(encpath, deviceEncodings, isPDF)))
		encoding = addEncoding(encpath, isPDF);
	    if (!encoding) {
		freeFontFamily(fontfamily);
		fontfamily = NULL;
	    } else {
		/*
		 * Load font info
		 */
		fontfamily->encoding = encoding;
		for(i = 0; i < 5 ; i++) {
		    type1fontinfo font = makeType1Font();
		    const char *afmpath = fontMetricsFileName(name, i, fontdbname);
		    if (!font) {
			freeFontFamily(fontfamily);
			fontfamily = NULL;
			break;
		    }
		    if (!afmpath) {
			freeFontFamily(fontfamily);
			fontfamily = NULL;
			break;
		    }
		    fontfamily->fonts[i] = font;
		    if (!PostScriptLoadFontMetrics(afmpath,
						   &(fontfamily->fonts[i]->metrics),
						   fontfamily->fonts[i]->name,
						   fontfamily->fonts[i]->charnames,
						   /*
						    * Reencode all but
						    * symbol face
						    */
						   encoding->encnames,
						   (i < 4)?1:0)) {
			warning(("cannot load afm file '%s'"), afmpath);
			freeFontFamily(fontfamily);
			fontfamily = NULL;
			break;
		    }
		}
		/*
		 * Add font
		 */
		if (fontfamily)
		    fontfamily = addLoadedFont(fontfamily, isPDF);
	    }
	}
    } else
	fontfamily = NULL;
    return fontfamily;
}

static type1fontlist addDeviceFont(type1fontfamily font,
				   type1fontlist devFonts,
				   int *index)
{
    type1fontlist newfont = makeFontList();
    *index = 0;
    if (!newfont) {
	devFonts = NULL;
    } else {
	type1fontlist fontlist = devFonts;
	newfont->family = font;
	*index = 1;
	if (!devFonts) {
	    devFonts = newfont;
	} else {
	    while (fontlist->next) {
		fontlist = fontlist->next;
		*index = *index + 1;
	    }
	    fontlist->next = newfont;
	}
    }
    return devFonts;
}

