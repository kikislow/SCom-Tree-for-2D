# SCom-Tree-for-2D Project

## About project
Compact representation of the height map.
The project is under development.

## Technologies
OpenGL, GLFW, GLAD, CMake

## Camera and window control
**W/S** - forward/backward 
**A/D** - left/right
**Q/E** - up/down
**Arrows** - camera rotation
**R** - reset to the original camera position
**Shift** - speeding up the movement
**ESC** - exit (completion of the program)

## Build Instructions

```bash
git clone --recursive https://github.com/kikislow/SCom-Tree-for-2D.git
cd SCom-Tree-For-2D
mkdir build && cd build
cmake ..
make
./SComTreeFor2D
