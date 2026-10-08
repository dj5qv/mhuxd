webuisveltedir = $(WEBUIDIR)/svelte

# The Svelte UI is built ahead of time on the maintainer's machine and its
# compiled output is committed under svelte/dist. Packaging therefore needs
# neither npm nor network access (e.g. offline builders like Launchpad PPAs).
# Run "make svelte-ui-build" to regenerate svelte/dist after editing the UI.
SVELTE_SRC_DIR = $(abs_srcdir)/svelte
SVELTE_DIST_DIR = $(srcdir)/svelte/dist
SVELTE_NPM ?= npm

EXTRA_DIST = \
	svelte/package.json \
	svelte/package-lock.json \
	svelte/vite.config.js \
	svelte/svelte.config.js \
	svelte/jsconfig.json \
	svelte/index.html \
	svelte/src \
	svelte/dist

# Install the pre-built Svelte UI. No npm/network access required.
# Normalize permissions after copying: the committed sources may be read-only
# (e.g. under "make distcheck"), and cp -R would otherwise carry that over and
# break uninstall.
install-data-local:
	$(MKDIR_P) "$(DESTDIR)$(webuisveltedir)"
	cp -R "$(SVELTE_DIST_DIR)"/. "$(DESTDIR)$(webuisveltedir)"/
	chmod -R u+rwX,go+rX,go-w "$(DESTDIR)$(webuisveltedir)"

uninstall-local:
	@rm -rf "$(DESTDIR)$(webuisveltedir)"

# Maintainer convenience target: rebuild svelte/dist from sources. This needs
# npm and network access and is only ever run on the developer's machine, never
# at package build time. The resulting svelte/dist is committed to the repo.
svelte-ui-build:
	cd "$(SVELTE_SRC_DIR)" && "$(SVELTE_NPM)" install && "$(SVELTE_NPM)" run build

.PHONY: svelte-ui-build