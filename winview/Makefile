all:
	$(CXX) -shared -fPIC --no-gnu-unique main.cpp overview.cpp OverviewPassElement.cpp -o winview.so -g `pkg-config --cflags pixman-1 libdrm hyprland pangocairo libinput libudev wayland-server xkbcommon` -std=c++2b -Wno-narrowing
clean:
	rm ./winview.so
