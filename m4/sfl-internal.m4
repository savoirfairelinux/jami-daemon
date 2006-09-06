AC_DEFUN([SFL_CXX_WITH_DEBUG],[

	AC_ARG_WITH(debug,
		AS_HELP_STRING(
			[--with-debug],
			[Set 'full' to enable debugging information @<:@default=no@:>@]
		),
		[with_debug=${withval}],
		[with_debug=no]
	)
	if test "x$with_debug" = "xfull" -o "x$with_debug" = "xyes"; then
		CXXFLAGS="$CXXFLAGS -g"
		CPPFLAGS="$CPPFLAGS -DSFLDEBUG"
	fi
])
