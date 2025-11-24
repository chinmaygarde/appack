build preset='debug':
	@cmake --build --preset {{preset}}

test preset='debug':
	@ctest --preset {{preset}}

alias gen := setup

setup preset='debug':
	cmake --preset {{preset}}

clean:
	rm -rf build

sync:
	git submodule update --init --recursive -j 8

list-builds:
	@cmake --list-presets
