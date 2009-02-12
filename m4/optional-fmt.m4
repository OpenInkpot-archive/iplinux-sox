dnl $1 package name                  e.g. flac 
dnl $2 package name in conditionals  e.g. FLAC 
dnl $3 header name                   e.g. FLAC/all.h 
dnl $4 check-lib name                e.g. FLAC 
dnl $5 check-lib fn-name             e.g. FLAC__stream_encoder_new 
dnl $6 libs1                         e.g. "-lFLAC $FLAC_LIBS $OGG_VORBIS_LIBS"
dnl $7 libs2                         e.g. "$FLAC_LIBS $OGG_VORBIS_LIBS"

AC_DEFUN([AC_OPTIONAL_FORMAT],
  [AC_ARG_WITH($1, AC_HELP_STRING([--with-$1=dyn], [load $1 dynamically]))
  using_$1=$with_$1
  if test "_$with_$1" = _dyn; then
    if test $using_libltdl != yes; then
      AC_MSG_FAILURE([not using libltdl; cannot load $1 dynamically])
    fi
  elif test "_$with_$1" = _; then
    using_$1=yes
  elif test "_$with_$1" != _yes -a "_$with_$1" != _no; then
    AC_MSG_FAILURE([invalid selection --with-$1=$with_$1])
  fi
  if test _$with_$1 != _no; then
    AC_CHECK_HEADER($3, [AC_CHECK_LIB($4, $5, $2_LIBS=$6,using_$1=no, $7)], using_$1=no)
    if test _$with_$1 != _ -a $using_$1 = no; then
      AC_MSG_FAILURE([cannot find $1])
    fi
  fi
  if test "$using_$1" != no; then
    AC_DEFINE(HAVE_$2, 1, [Define to 1 if you have $1.])
    if test "$using_$1" = yes; then
      AC_DEFINE(STATIC_$2, 1, [Define to 1 if you have static $1.])
    fi
  fi
  AM_CONDITIONAL(HAVE_$2, test $using_$1 != no)
  AM_CONDITIONAL(STATIC_$2, test $using_$1 = yes)
  AC_SUBST($2_CFLAGS)
  AC_SUBST($2_LIBS)]
)
