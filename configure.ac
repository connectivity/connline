dnl # ##########################
dnl Libconnline configure script
dnl # ##########################

dnl Process this file with autoconf to produce a configure script.

AC_INIT(libconnline, 0.1)

AC_CONFIG_SRCDIR([src/connline.c])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([config/m4])
AM_CONFIG_HEADER([include/config.h])

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AM_MAINTAINER_MODE

AC_SUBST(abs_top_srcdir)

AC_PROG_MAKE_SET
AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_PROG_INSTALL
AM_PROG_LIBTOOL


dnl # ###########
dnl Check headers
dnl # ###########

AC_HEADER_STDC
AC_HEADER_STDBOOL

AC_CHECK_HEADERS([stdio.h time.h sys/stat.h sys/types.h unistd.h])


dnl # ##############
dnl Checks functions
dnl # ##############

AC_CHECK_FUNCS([calloc realloc memset strncpy snprintf strncmp strlen free strchr strrchr getpid time rand])


dnl # ########
dnl pkg-config
dnl # ########

AC_CHECK_PROG(HAVE_PKG_CONFIG, pkg-config, yes, no)

if test "x$HAVE_PKG_CONFIG" = "xno"; then
   AC_MSG_ERROR([You need to install pkg-config tool])
fi


dnl # ##########
dnl DBus library
dnl # ##########

PKG_CHECK_MODULES(DBUS, dbus-1, [], [AC_MSG_ERROR([You need to install DBus])])


dnl # #############
dnl Debug/Test mode
dnl # #############

AC_ARG_ENABLE([debug],
		[AS_HELP_STRING([--enable-debug], [Enable debug mode])],
		[
		if test "x$enable_debug" = "xyes"; then
			CFLAGS="$CFLAGS -g"
		fi
		],
		[enable_debug=no]
		)


dnl # ########
dnl Event loop
dnl # ########

dnl Glib support
AC_ARG_ENABLE([glib], [AS_HELP_STRING([--enable-glib], [Enable 'glib' event loop support])], [], [enable_glib=yes])
PKG_CHECK_MODULES(GLIB, glib-2.0, [], [enable_glib=no])
AM_CONDITIONAL([CONNLINE_EVENT_GLIB], [test "x$enable_glib" = "xyes"])

dnl EFL support
AC_ARG_ENABLE([efl], [AS_HELP_STRING([--enable-efl], [Enable 'efl' Ecore event loop support])], [], [enable_efl=yes])
PKG_CHECK_MODULES(EFL, ecore, [], [enable_efl=no])
AM_CONDITIONAL([CONNLINE_EVENT_EFL], [test "x$enable_efl" = "xyes"])

dnl Libevent support
AC_ARG_ENABLE([libevent], [AS_HELP_STRING([--enable-libevent], [Enable 'libevent' event loop support])], [], [enable_libevent=yes])
AC_CHECK_LIB([event], [event_init], [], [enable_libevent=no])
AM_CONDITIONAL([CONNLINE_EVENT_LIBEVENT], [test "x$enable_libevent" = "xyes"])
if test "x$enable_libevent" = "xyes"; then
	if test "x$enable_glib" = "xno"; then
		AC_MSG_ERROR([You need Glib support to enable the libevent support])
	fi

	LIBEVENT_CFLAGS=""
	LIBEVENT_LIBS="-levent"

	AC_SUBST([LIBEVENT_CFLAGS], "$LIBEVENT_CFLAGS")
	AC_SUBST([LIBEVENT_LIBS], "$LIBEVENT_LIBS")
fi

AC_SUBST([CONNLINE_PKG_CONFIG_LDFLAGS], "$DBUS_LIBS")
AC_SUBST([CONNLINE_PKG_CONFIG_CFLAGS], "$DBUS_CFLAGS")


dnl # ######
dnl Backends
dnl # ######

dnl ConnMan support
AC_ARG_ENABLE([connman], [AS_HELP_STRING([--enable-connman], [Enable 'ConnMan' backend])], [], [enable_connman=no])
AM_CONDITIONAL([CONNLINE_BACKEND_CONNMAN], [test "x$enable_connman" = "xyes"])

dnl NetworkManager support
AC_ARG_ENABLE([nm], [AS_HELP_STRING([--enable-nm], [Enable 'Network Manager' backend])], [], [enable_nm=no])
AM_CONDITIONAL([CONNLINE_BACKEND_NM], [test "x$enable_nm" = "xyes"])

dnl Wicd support
AC_ARG_ENABLE([wicd], [AS_HELP_STRING([--enable-wicd], [Enable 'Wicd' backend])], [], [enable_wicd=no])
AM_CONDITIONAL([CONNLINE_BACKEND_WICD], [test "x$enable_wicd" = "xyes"])


dnl # ###
dnl Tests
dnl # ###

AC_ARG_ENABLE([test],
		[AS_HELP_STRING([--enable-test], [Enable test mode])],
		[],[enable_test=no])
AM_CONDITIONAL([TEST], [test "x$enable_test" = "xyes"])


dnl # #############
dnl # Makefile list
dnl # #############

AC_CONFIG_FILES([
Makefile
connline.pc
])


dnl # ############
dnl # Final Output
dnl # ############

prefix_to_print=$prefix
if test "x$prefix" = "xNONE"; then
	prefix_to_print=$ac_default_prefix
fi

AC_MSG_RESULT([
	Libconnline configuration
	=========================

	Install prefix           : $prefix_to_print
	Debug                    : $enable_debug
	Test                     : $enable_test

	Backends:
	--------

	ConnMan                  : $enable_connman
	NetworkManager           : $enable_nm
	Wicd                     : $enable_wicd

	Event loop:
	----------

	Glib                     : $enable_glib
	EFL (Ecore)              : $enable_efl
	Libevent                 : $enable_libevent
])

AC_OUTPUT

