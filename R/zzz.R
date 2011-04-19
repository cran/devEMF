.First.lib <- function(libloc, pkg) {
  library.dynam('devEMF', pkg, libloc)
}
.Last.lib <- function(libpath) {
  library.dynam.unload('devEMF', libpath)
}
