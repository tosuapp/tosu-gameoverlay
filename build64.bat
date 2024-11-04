cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DDESKTOP=1 -DA64=1 -B build
ninja -C build

cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DDESKTOP=0 -DA64=1 -B build
ninja -C build
