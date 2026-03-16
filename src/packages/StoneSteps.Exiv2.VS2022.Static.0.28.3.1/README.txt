This package contains static Exiv2 libraries and header files
for the x64 platform built with Visual C++ 2022, against
Debug/Release MT/DLL MSVC CRT.

The Exiv2 static libraries from this package will appear within
the installation target project after the package is installed.
The solution may need to be reloaded to make libraries visible.
Both, debug and release libraries will be listed in the project,
but only the one appropriate for the currently selected
configuration will be included in the build. These libraries
may be moved into solution folders after the installation (e.g.
lib/Debug and lib/Release).

Note that the Exiv2 library path in this package will be selected
as Debug or Release based on whether the selected configuration
is designated as a development or as a release configuration via
the standard Visual Studio property called UseDebugLibraries.
Additional configurations copied from the standard ones will
inherit this property. 

Do not install this package if your projects use debug configurations
without UseDebugLibraries. Note that CMake-generated Visual Studio
projects will not emit this property.

The Exiv2 libraries call functions in these Windows libraries,
which need to be added as linker input to the Visual Studio
project using this package.

 * psapi.lib
 * ws2_32.lib
 * shell32.lib

See README.md in Exiv2-Nuget project for more details.

https://github.com/StoneStepsInc/exiv2-nuget
