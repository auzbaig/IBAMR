## Process this file with automake to produce Makefile.in
include $(top_srcdir)/config/Make-rules
SUBDIRS = IB adv_diff advect navier_stokes

## Standard make targets.
examples:
	@(cd IB            && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1;
	@(cd adv_diff      && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1;
	@(cd advect        && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1;
	@(cd navier_stokes && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1;