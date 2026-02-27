hdfdir = $(HDFDIR)
webuistaticdir = $(WEBUIDIR)/static
webuicsdir = $(WEBUIDIR)/cs
webuisveltedir = $(WEBUIDIR)/svelte

SVELTE_SRC_DIR = $(abs_srcdir)/svelte
SVELTE_DIR = $(abs_builddir)/svelte
SVELTE_BUILD_DIR = $(SVELTE_DIR)/dist
SVELTE_NPM ?= npm

dist_webuistatic_DATA = \
	static/monda400.woff \
	static/monda700.woff \
	static/mhuxd.css \
	static/arrow-down.png \
	static/arrow-up.png \
	static/big_bullet.gif \
	static/checkmark.gif \
	static/contexthelp2.gif \
	static/dot.gif \
	static/error.png \
	static/Green1.png \
	static/Green2.png \
	static/Green3.png \
	static/mhuxd-logo.gif \
	static/mhuxd.png \
	static/navtab2-left.gif \
	static/navtab2-right.gif \
	static/navtab2-sel-left.gif \
	static/navtab2-sel-right.gif \
	static/nocheckmark.gif \
	static/note.gif \
	static/Red1.png \
	static/section-end-corner2.gif \
	static/section-end-horiz.gif \
	static/section-end-vert.gif \
	static/section-start-corner2.gif \
	static/section-start-horiz.gif \
	static/section-start-vert.gif \
	static/stop.gif \
	static/warning.gif \
	static/Yellow.png \
	static/scrollfix.js

dist_webuics_DATA = \
	cs/messages.cs \
	cs/messages_box.cs \
	cs/audio_mk1.cs \
	cs/audio_mk2.cs \
	cs/audio_mk2_opts.cs \
	cs/audio_mk2r_r1.cs \
	cs/audio_mk2r_r2.cs \
	cs/daemon_ports.cs \
	cs/daemon_settings.cs \
	cs/display_cfg.cs \
	cs/display_ev.cs \
	cs/home.cs \
	cs/home_summary.cs \
	cs/keyer_audiosw.cs \
	cs/keyer_aux.cs \
	cs/keyer_display.cs \
	cs/keyer_fsk.cs \
	cs/keyer_mode.cs \
	cs/keyer_ptt.cs \
	cs/keyer_radio.cs \
	cs/keyer_summary.cs \
	cs/keyer_winkey.cs \
	cs/left_menu.cs \
	cs/macros.cs \
	cs/mode_cfg.cs \
	cs/pageheader.cs \
	cs/ptt_cfg.cs \
	cs/ptt_footsw.cs \
	cs/sectionheader.cs \
	cs/serial_cfg.cs \
	cs/sidetone_cfg.cs \
	cs/summary_cfg.cs \
	cs/winkey_cfg.cs \
	cs/sm_antsw_outputs.cs \
	cs/sm_antsw_settings.cs \
	cs/sm_antsw_ant_list.cs \
	cs/sm_antsw_vr_list.cs \
	cs/sm_antsw_grp_list.cs \
	cs/sm_antsw_band_list.cs \
	cs/sm_antsw_load_store.cs

EXTRA_DIST = \
	svelte/package.json \
	svelte/package-lock.json \
	svelte/vite.config.js \
	svelte/svelte.config.js \
	svelte/index.html \
	svelte/src \
	svelte/public

svelte-ui-build:
	@echo "Starte Svelte Build Prozess..."
	@if test "$(abs_srcdir)" != "$(abs_builddir)"; then \
		$(MKDIR_P) "$(SVELTE_DIR)"; \
		cp -R "$(SVELTE_SRC_DIR)"/package*.json "$(SVELTE_SRC_DIR)"/*config.js "$(SVELTE_SRC_DIR)"/index.html "$(SVELTE_SRC_DIR)"/src "$(SVELTE_SRC_DIR)"/public "$(SVELTE_DIR)"/; \
		chmod -R u+w "$(SVELTE_DIR)"; \
	fi
	cd "$(SVELTE_DIR)" && "$(SVELTE_NPM)" install && "$(SVELTE_NPM)" run build

install-data-local:
	$(MKDIR_P) "$(DESTDIR)$(webuisveltedir)"
	cp -R "$(SVELTE_BUILD_DIR)"/. "$(DESTDIR)$(webuisveltedir)"/

all-local: svelte-ui-build

uninstall-local:
	@rm -rf "$(DESTDIR)$(webuisveltedir)"

clean-local:
	rm -f svelte-ui-build
	rm -rf "$(SVELTE_BUILD_DIR)"

distclean-local:
	@if test "$(abs_srcdir)" != "$(abs_builddir)"; then \
		rm -rf "$(SVELTE_DIR)"; \
	else \
		rm -rf "$(SVELTE_BUILD_DIR)" "$(SVELTE_DIR)/node_modules"; \
	fi

# For automake
.PHONY: svelte-ui-build