.PHONY: all icon host-check profile-check icon-check preview package release clean
# Your own stock 6.61 resource; the cassette icon patch is applied to it.
ICON_SOURCE ?= captures/run_000/visualizer_plugin.rco

all:
	mkdir -p build
	$(MAKE) -f Makefile.render
	$(MAKE) -f Makefile.plugin
	python3 tools/audit_imports.py
	@if [ -f visualizer.rco ]; then :; \
	elif [ -f "$(ICON_SOURCE)" ]; then $(MAKE) visualizer.rco; \
	else echo "NOTE: no stock resource at $(ICON_SOURCE); building without the cassette icon."; \
	     echo "      Run: make ICON_SOURCE=/path/to/visualizer_plugin.rco"; fi
	python3 tools/checksums.py

# Cassette selector icon, created from the stock resource with the shipped patch.
visualizer.rco: assets/visualizer-icon.patch.json
	rm -f visualizer.rco
	python3 tools/apply_icon_patch.py "$(ICON_SOURCE)" --output visualizer.rco

icon:
	mkdir -p build
	python3 tools/build_icon.py
	python3 tools/build_icon_patch.py
	cp build/visualizer.rco visualizer.rco

# Checks the install copy of visualizer.rco without regenerating anything.
icon-check: visualizer.rco
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Itests/psp_stubs icon.c tests/icon_test.c -o build/icon_test
	build/icon_test captures/run_000/music_player_module.seg0.bin visualizer.rco
	python3 tests/icon_resource_test.py

host-check:
	mkdir -p build
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined tape.c animation.c tests/render_test.c -o build/render_test
	build/render_test
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined animation.c tests/animation_test.c -o build/animation_test
	build/animation_test
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined tape.c animation.c mesh.c tests/mesh_test.c -o build/mesh_test
	build/mesh_test
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined tape.c animation.c mesh.c frame.c tests/frame_test.c -o build/frame_test
	build/frame_test
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined safety.c tests/safety_test.c -o build/safety_test
	build/safety_test
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Itests/psp_stubs ticket.c tests/ticket_test.c -o build/ticket_test
	build/ticket_test
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined -Itests/psp_stubs guard.c tests/guard_test.c -o build/guard_test
	build/guard_test
	python3 tests/icon_patch_test.py

profile-check:
	mkdir -p build
	cc -std=c99 -O2 -Wall -Wextra -Werror -g -fsanitize=address,undefined profile.c tests/profile_test.c -o build/profile_test
	build/profile_test captures/run_000/visualizer_module.seg0.bin

preview:
	mkdir -p build
	cc -std=c99 -Wall -Wextra -Werror -O2 tape.c animation.c tools/render_preview.c -o build/render_preview
	build/render_preview 440 > build/cassette.ppm

package: all
	python3 tools/package_release.py

release: package

clean:
	$(MAKE) -f Makefile.render clean
	$(MAKE) -f Makefile.plugin clean
	rm -f SHA256SUMS
