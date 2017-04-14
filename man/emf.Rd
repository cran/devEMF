\name{emf}
\Rdversion{1.1}
\alias{emf}
\alias{devEMF}
\title{Enhanced Metafile Graphics Device}
\description{
  'emf' starts the graphics device driver for producing enhanced
  metafile (emf+/emf) graphics, which can imported natively as vector
  graphics in both LibreOffice and Microsoft Office.
}
\usage{

emf(file = "Rplot.emf", width = 7, height = 7,
    bg = "transparent", fg = "black", pointsize = 12,
    family = "Helvetica", coordDPI = 300, custom.lty=emfPlus,
    emfPlus=TRUE, emfPlusFont = FALSE, emfPlusRaster = FALSE)
}

\arguments{
  \item{file}{character string giving the name of file.}
  \item{width}{width of plot in inches.}
  \item{height}{height of plot in inches.}
  \item{bg}{plot background color.  If \code{"transparent"}, no background
    color is painted.}
  \item{fg}{initial foreground color to be used.}
  \item{pointsize}{default font point size to be used.}
  \item{family}{default font family to be used.}
  \item{coordDPI}{DPI used for coordinate system (while EMF is a vector
    format, it (and R) use discrete coordinate systems).}
  \item{custom.lty}{logical: if false, approximate R line types with EMF
    standard line types (any program that imports EMF should support
    this).  If true, use exact (custom) R line type, which is only
    supported by LibreOffice if using EMF+.}
  \item{emfPlus}{logical: whether to use EMF+ records or not}
  \item{emfPlusFont}{logical: if using EMF+, should text use EMF+ or EMF
    records?}
  \item{emfPlusRaster}{logical: if using EMF+, should raster images use
    EMF+ or EMF records?}
}
\details{
  The standard office suites support very few vector graphics formats
  for import.  Enhanced Metafiles (EMFs) do tend to be supported, which
  is the purpose of this device.  EMF+ is a newer format that is
  encapsulated within an EMF file and allows options such as partial
  transparency.

  The EMF/EMF+ specifications do not allow for multiple pages; attempting to
  create multiple pages will result in a warning message and the output
  file will contain the different pages superimposed.
  
  Also, EMF/EMF+ do not allow for embedding fonts.  Thus you must be
  careful that your desired font is present both on the system that you
  use when you create the emf file (i.e., when you run R) and on the
  system that you use to import/view the resulting file.  Sticking to
  the standard Adobe PostScript font families is probably a good idea.

  EMF/EMF+ supports Unicode characters, and this package tries to maintain
  support as well.  However, font metric information is system dependent
  and so support may be spotty in practice.  Basic font metrics for the
  standard Adobe PostScript font families are built into this package.

  Only EMF+ allows partial transparency (i.e., the only useful type --
  0.0 < alpha < 1.0); attempting to use a transparent color when
  \code{emfPlus = FALSE} will result in a warning message and the output
  will be completely transparent (invisible).

  LibreOffice support for EMF+ remains incomplete as of version 5, and,
  cannot handle EMF+ records with rotated text or raster images (hence
  the options to turn off EMF+ and use EMF instead for these types of
  records).  devEMF defaults to EMF for these records to maintain
  compatibility, but quality is higher if EMF+ records are used.
}
\section{Known limitations}{
  \itemize{
    \item EMF (as opposed to EMF+) raster rendering does not support
  interpolation control.
    \item EMF (as opposed to EMF+) does not support an alpha channel.
    \item Path rendering is not implemented for EMF (as opposed to EMF+).
    \item Clipping is not implemented, since R \emph{appears} to take
  care of this; however, if you see a problem that could be the result a
  clipping bug, please contact the author.
  }
}
\author{
  Philip Johnson
}
\seealso{
  \code{\link{Devices}}
}
\examples{
require(devEMF)
\dontrun{
# open file "bar.emf" for graphics output
emf("bar.emf")
# produce the desired graph(s)
plot(1,1)
dev.off() #turn off device and finalize file
}
}
% Add one or more standard keywords, see file 'KEYWORDS' in the
% R documentation directory.
\keyword{device}
