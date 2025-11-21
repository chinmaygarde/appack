build:
	cmake --build --preset default

setup:
	cmake --preset default

clean:
	rm -rf build

sync:
	git submodule update --init --recursive -j 8
