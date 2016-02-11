
SUBDIRS = src
AUTOMAKE_OPTIONS = foreign

install-data-hook:
	( cd dist; ./install.sh "$(DESTDIR)$(prefix)";)
	
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS}
