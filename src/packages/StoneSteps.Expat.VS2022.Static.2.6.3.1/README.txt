This package contains static Expat libraries and header files
for the x64 platform, built with Visual C++ 2022, against
Debug/Release MT/DLL MSVC CRT.

The Expat static libraries from this package will appear within
the installation target project after the package is installed.
The solution may need to be reloaded to make libraries visible.
Both, debug and release libraries will be listed in the project,
but only the one appropriate for the currently selected
configuration will be included in the build. These libraries
may be moved into solution folders after the installation (e.g.
lib/Debug and lib/Release).

Note that the Expat library path in this package will be selected
as Debug or Release based on whether the selected configuration
is designated as a development or as a release configuration via
the standard Visual Studio property called UseDebugLibraries.
Additional configurations copied from the standard ones will
inherit this property. 

Do not install this package if your projects use debug configurations
without UseDebugLibraries. Note that CMake-generated Visual Studio
projects will not emit this property.

Expat libraries in this package are built with narrow character
support only and cannot parse XML documents maintained as wide
character strings.
