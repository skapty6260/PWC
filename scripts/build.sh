export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig
export LD_LIBRARY_PATH=/usr/local/lib

meson setup build/
meson compile -c build/
cd build
sudo ninja install