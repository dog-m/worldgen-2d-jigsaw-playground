cd ./build
conan install .. --build=missing
cmake .. -G "Visual Studio 17"
cmake --build . --config Release
pause