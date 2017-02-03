# Process this file with autoconf to produce a configure script.
AC_INIT(devEMF, 2)

# find R home and set CXX/CXXFLAGS
: ${R_HOME=`R RHOME`}
if test -z "${R_HOME}"; then
  echo "could not determine R_HOME"
  exit 1
fi
RBIN="${R_HOME}/bin/R"
CXX=`"${RBIN}" CMD config CXX`;
CXXFLAGS=`"${RBIN}" CMD config CXXFLAGS`
CPP=`"${RBIN}" CMD config CPP`;
AC_SUBST(R_HOME)

AC_LANG(C++)

#-----------------------------------------------------------------------
if test `uname -s` != "Darwin"; then #only necessary if NOT on OSX
   AC_PATH_PROG([PKGCONF],[pkg-config],[],[$PATH:/usr/local/bin:ext/bin:ext:/sw/bin:/opt/bin])

   if test "xx$XFT_LIBS" == "xx"; then
      if test "xx$PKGCONF" != "xx"; then
         AC_MSG_CHECKING([whether pkg-config knows about xft])
         if "${PKGCONF}" --exists xft; then
            AC_MSG_RESULT([yes])
            XFT_CFLAGS=`"${PKGCONF}" --cflags xft`
            XFT_LIBS=`"${PKGCONF}" --libs xft`
         else
            AC_MSG_RESULT([no])
         fi
      fi
   fi

   AC_MSG_NOTICE([XFT_CFLAGS=${XFT_CFLAGS}])
   AC_MSG_NOTICE([XFT_LIBS=${XFT_LIBS}])


   if test "xx$Z_LIBS" == "xx"; then
      if test "xx$PKGCONF" != "xx"; then
         AC_MSG_CHECKING([whether pkg-config knows about zlib])
         if "${PKGCONF}" --exists zlib; then
            AC_MSG_RESULT([yes])
            ZLIB_CFLAGS=`"${PKGCONF}" --cflags zlib`
            ZLIB_LIBS=`"${PKGCONF}" --libs zlib`
         else
            AC_MSG_RESULT([no])
         fi
      fi
   fi


   CPPFLAGS="${XFT_CFLAGS} ${ZLIB_CFLAGS}"
   LIBS="${XFT_LIBS} ${ZLIB_LIBS}"

   AC_CHECK_HEADERS_ONCE(X11/Xft/Xft.h)
   AC_CHECK_HEADERS_ONCE(zlib.h)

   if (test "x$ac_cv_header_zlib_h" == xyes); then
      CPPFLAGS="${CPPFLAGS} -DHAVE_ZLIB"
   fi
   if (test "x$ac_cv_header_X11_Xft_Xft_h" == xyes); then
      CPPFLAGS="${CPPFLAGS} -DHAVE_XFT"
   fi

   if test "x$ac_cv_header_zlib_h" == xno  &&  test "x$ac_cv_header_X11_Xft_Xft_h" == xno; then
      AC_MSG_ERROR([Cannot find Xft.h or zlib.h! Please install the Xft or Zlib development headers and/or set XFT_CFLAGS/LIBS or ZLIB_CFLAGS/LIBS correspondingly.])
   fi

fi

AC_CONFIG_FILES([src/Makevars])
AC_OUTPUT
