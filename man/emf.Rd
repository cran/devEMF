\name{emf}
\Rdversion{1.1}
\alias{emf}
\alias{devEMF}
\title{Enhanced Metafile Graphics Device}
\description{
  'emf' starts the graphics device driver for producing enhanced
  metafile (emf) graphics, which can imported natively as vector
  graphics in both OpenOffice and Microsoft Office.
}
\usage{
emf(file = "Rplot.emf", width = 7, height = 7,
    bg = "transparent", fg = "black", pointsize = 12,
    family = "Helvetica", custom.lty = FALSE)
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
  \item{custom.lty}{logical: if false, approximate R line types with EMF
    standard line types (any program that imports EMF should support
    this).  If true, use exact R line type via the EMF \code{"PS_USERSTYLE"}
    option, which does not appear to be supported by OpenOffice.}
}
\details{
  The standard office suites support very few vector graphics formats
  for import.  Enhanced Metafiles (EMFs) do tend to be supported, which
  is the purpose of this device.

  The EMF specification does not allow for multiple pages; attempting to
  create multiple pages will result in a warning message and the output
  file will contain the different pages superimposed.
  
  Also, EMF does not allow for embedding fonts.  Thus you must be
  careful that your desired font is present both on the system that you
  use when you create the emf file (i.e., when you run R) and on the
  system that you use to import/view the resulting file.  Sticking to
  the standard Adobe PostScript font families is probably a good idea.

  EMF has full Unicode character support, and this package tries to
  maintain full support as well.  However, the font metric information
  is \emph{clunkily} borrowed from the R postscript device and will not
  to work properly for non-Latin character sets.

  The EMF format itself does not allow for partial transparency (i.e.,
  the only useful type -- 0.0 < alpha < 1.0); attempting to use a
  transparent color will result in a warning message and the output will
  be completely transparent (invisible).
  
  EMF-level clipping is not yet implemented, since R \emph{appears} to
  take care of this; however, if you see a problem that could be the
  result a clipping bug, please contact the author.
}
\note{
  This package copies the postscript font metric code from the R core
  grDevices library and uses the AFM files installed with that library.
  Thanks to the R core development team for writing this code.
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
dev.off() #turn off device and finalize file
}
}
% Add one or more standard keywords, see file 'KEYWORDS' in the
% R documentation directory.
\keyword{device}
