# first we compile exe
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DDESKTOP=1 -B build
ninja -C build

# then we compile dll
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DDESKTOP=0 -B build
ninja -C build
