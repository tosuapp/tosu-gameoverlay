cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DDESKTOP=1 -DA64=0 -B build
ninja -C build

cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DDESKTOP=0 -B build
ninja -C build
