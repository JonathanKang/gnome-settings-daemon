plugin_name = updates

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = org.gnome.SettingsDaemon.Updates.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

org.gnome.SettingsDaemon.Updates.desktop: $(desktop_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

polkit_policydir = $(datadir)/polkit-1/actions
polkit_policy_DATA = org.gnome.settings-daemon.plugins.update.policy

libexec_PROGRAMS = gsd-updates pk-clear-offline-update

pk_clear_offline_update_SOURCES = pk-offline-clear-update.c \
	gsd-updates-common.h

pk_clear_offline_update_CFLAGS =		\
	$(PLUGIN_CFLAGS)		\
	`$(PKG_CONFIG) --cflags "glib-2.0"`

gsd_updates_SOURCES =		\
	main.c			\
	gsd-updates-common.h		\
	gsd-updates-refresh.h		\
	gsd-updates-refresh.c		\
	gsd-updates-firmware.h		\
	gsd-updates-firmware.c		\
	gsd-updates-manager.h		\
	gsd-updates-manager.c

gsd_updates_CPPFLAGS =					\
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/gnome-settings-daemon			\
	-I$(top_builddir)/gnome-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	$(AM_CPPFLAGS)

gsd_updates_CFLAGS =			\
	-DI_KNOW_THE_PACKAGEKIT_GLIB2_API_IS_SUBJECT_TO_CHANGE \
	-DDATADIR=\"$(datadir)\"	\
	-DBINDIR=\"$(bindir)\"		\
	`$(PKG_CONFIG) --cflags "gtk+-3.0 glib-2.0 gsettings-desktop-schemas"`	\
	$(PLUGIN_CFLAGS)		\
	$(PACKAGEKIT_CFLAGS)		\
	$(AM_CFLAGS)

gsd_updates_LDADD =						\
	$(top_builddir)/plugins/common/libcommon.la 		\
	$(top_builddir)/gnome-settings-daemon/libgsd.la		\
	$(SETTINGS_DAEMON_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)			\
	$(PACKAGEKIT_LIBS)			\
	-lm

EXTRA_DIST = $(desktop_in_files)

CLEANFILES =					\
	$(desktop_DATA)				\
	org.gnome.settings-daemon.plugins.update.policy

DISTCLEANFILES = $(desktop_DATA)
