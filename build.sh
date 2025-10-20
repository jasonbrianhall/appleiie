g++ -o appleiie main.cpp instructions.cpp disk.cpp `pkg-config --cflags --libs gtk+-3.0` -DWITH_GTK -lncurses -std=c++17
