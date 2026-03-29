[![Documentation](https://img.shields.io/badge/Docs-Outdated-yellowgreen)](https://htmlpreview.github.io/?https://github.com/sylikc/jpegview/blob/master/src/JPEGView/Config/readme.html) [![Localization Progress](https://img.shields.io/badge/Localized-91%25-blueviolet)](#Localization) [![Build x64](https://github.com/sylikc/jpegview/actions/workflows/build-release-x64.yml/badge.svg?branch=master)](https://github.com/sylikc/jpegview/actions/workflows/build-release-x64.yml) [![OS Support](https://img.shields.io/badge/Windows-XP%20%7C%207%20%7C%208%20%7C%2010%20%7C%2011-blue)](#) [![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue)](https://github.com/sylikc/jpegview/blob/master/LICENSE.txt)

[![Latest GitHub Release](https://img.shields.io/github/v/release/sylikc/jpegview?label=GitHub&style=social)](https://github.com/sylikc/jpegview/releases)[![Downloads](https://badgen.net/github/assets-dl/sylikc/jpegview?cache=3600&color=grey&label=)](#) [![WinGet](https://repology.org/badge/version-for-repo/winget/jpegview.svg?allow_ignored=1&header=WinGet)](https://winstall.app/apps/sylikc.JPEGView) [![PortableApps](https://img.shields.io/badge/PortableApps-Current-green)](https://portableapps.com/apps/graphics_pictures/jpegview_portable) [![Scoop](https://repology.org/badge/version-for-repo/scoop/jpegview-fork.svg?header=Scoop)](https://scoop.sh/#/apps?q=%22jpegview-fork%22) [![Chocolatey](https://img.shields.io/chocolatey/v/jpegview)](https://community.chocolatey.org/packages/jpegview) [![Npackd](https://repology.org/badge/version-for-repo/npackd_stable/jpegview.svg?allow_ignored=1&header=Npackd)](https://www.npackd.org/p/jpegview)

# JPEGView - Image Viewer and Editor

This is a fork of David Kleiner's excellent and feature-rich JPEGView.

### New Features

- **Exiv2 Integration**: Uses the Exiv2 library to extract EXIF/XMP metadata from all image file types (JPG, RAW, etc.)
- **Compact EXIF Display**: View EXIF data in a streamlined format by setting `ShowCompactEXIF=true` in your JPEGViewer.ini file

### Bug Fixes

- **ICC Profile Support**: Enabled ICM for monitor color management, ensuring colors are rendered according to your Windows ICC profile configuration
- **Display Bug Resolution**: Fixed an issue where images were displayed twice in certain scenarios (see [issue #147](https://github.com/sylikc/jpegview/issues/147))

### Build Notes

- Solution migrated to Visual Studio 2022
- 32-bit build configurations removed
- Exiv2 library (v0.28.3.1) is automatically downloaded from NuGet during the build process


## Description

JPEGView is a lean, fast and highly configurable image viewer/editor with a minimal GUI.

### Formats Supported

JPEGView has built-in support the following formats:

* Popular: JPEG, GIF
* Lossless: BMP, PNG, TIFF, PSD
* Web: WEBP, JXL, HEIF/HEIC, AVIF
* Legacy: TGA, WDP, HDP, JXR
* Camera RAW formats:
  * Adobe (DNG), Canon (CRW, CR2, CR3), Nikon (NEF, NRW), Sony (ARW, SR2)
  * Olympus (ORF), Panasonic (RW2), Fujifilm (RAF)
  * Sigma (X3F), Pentax (PEF), Minolta (MRW), Kodak (KDC, DCR)
  * A full list is available here: [LibRaw supported cameras](https://www.libraw.org/supported-cameras)

Many additional formats are supported by Windows Imaging Component (WIC)

### Basic Image Editor

Basic on-the-fly image processing is provided - allowing adjusting typical parameters:

* sharpness
* color balance
* rotation
* perspective
* contrast
* local under-exposure/over-exposure

### Other Features

* Small and fast, uses AVX2/SSE2 and up to 4 CPU cores
* High quality resampling filter, preserving sharpness of images
* Basic image processing tools can be applied realtime during viewing
* Movie/Slideshow mode - to play folder of JPEGs as movie

### Installation

This fork does not provide pre-built installers or distributions. To use it, you must build from source.
See [COMPILING.txt](https://github.com/gm-h28669/jpegview-exiv2/blob/master/COMPILING.txt) for details.

The build process generates the following outputs:

**Executable Files**
- **Debug build**: `.\src\JPEGView\bin\x64\Debug`
- **Release build**: `.\src\JPEGView\bin\x64\Release`

**Installer (.msi)**
- **Debug build**: `.\src\JPEGView.Setup\bin\x64\Debug\en-us`
- **Release build**: `.\src\JPEGView.Setup\bin\x64\Release\en-us`

See [HowToInstall.txt](https://github.com/gm-h28669/jpegview-exiv2/blob/master/HowToInstall.txt) for details on how to install/uninstall JPEGView after building.

### System Requirements

- **Operating System**: Windows 7, 8, 10, or 11 (64-bit)
- **Architecture**: 64-bit only (32-bit not supported in this fork, mainly because nowadays CPUs are 64 bit)

## What's New

Check the [CHANGELOG.txt](https://github.com/gm-h28669/jpegview-exiv2/blob/master/README.md) to review new features in detail.

# Localization

By default, the language is auto-detected to match your Windows Locale.  All the text in the menus and user interface should show in your language.  To override the auto-detection, manually set `Language` option in `JPEGView.ini`

JPEGView is currently translated/localized to 28 languages:

| INI Option | Language |
| ---------- | -------- |
| be | Belarusian |
| bg | Bulgarian |
| cs | Czech |
| de | German |
| el | Greek, Modern |
| es-ar | Spanish (Argentina) |
| es | Spanish |
| eu | Basque |
| fi | Finnish |
| fr | French (Français) |
| hu | Hungarian |
| it | Italian |
| ja | Japanese (日本語) |
| ko | Korean (한국어) |
| pl | Polish |
| pt-br | Portuguese (Brazilian) |
| pt | Portuguese |
| ro | Romanian |
| ru | Russian (Русский) |
| sk | Slovak |
| sl | Slovenian (Slovenščina) |
| sr | Serbian (српски) |
| sv | Swedish |
| ta | Tamil |
| tr | Turkish (Türkçe) |
| uk | Ukrainian (Українська) |
| zh-tw | Chinese, Traditional (繁體中文) |
| zh | Chinese, Simplified (简体中文) |

See the [Localization wiki page](https://github.com/sylikc/jpegview/wiki/Localization#localization-status) for translation status for each language.

# Help / Documentation

The JPEGView documentation is a little out of the date at the moment, but should still give a good summary of the features.

This [readme.html](https://htmlpreview.github.io/?https://github.com/sylikc/jpegview/blob/master/src/JPEGView/Config/readme.html) is part of the JPEGView package.

# Brief History

This GitHub repo continues the legacy (is a "fork") of the excellent project [JPEGView by David Kleiner](https://sourceforge.net/projects/jpegview/).  Unfortunately, starting in 2020, the SourceForge project has essentially been abandoned, with the last update being [2018-02-24 (1.0.37)](https://sourceforge.net/projects/jpegview/files/jpegview/).  It's an excellent lightweight image viewer that I use almost daily!

The starting point for this repo was a direct clone from SourceForge SVN to GitHub Git.  By continuing this way, it retains all previous commits and all original author comments.

I'm hoping with this project, some devs might help me keep the project alive!  It's been awhile, and could use some new features or updates.  Looking forward to the community making suggestions, and devs will help with some do pull requests as some of the image code is quite a learning curve for me to pick it up. -sylikc

## Special Thanks

Special thanks to [qbnu](https://github.com/qbnu) for adding additional codec support!
* Animated WebP
* Animated PNG
* JPEG XL with animation support
* HEIF/HEIC/AVIF support
* QOI support
* ICC Profile support for WebP, JPEG XL, HEIF/HEIC, AVIF
* LibRaw support (all updated RAW formats, such as CR3)
* Photoshop PSD support

Thanks to all the _translators_ which keep JPEGView strings up-to-date in different languages!  See [CHANGELOG.txt](https://github.com/gm-h28669/jpegview-exiv2/blob/master/CHANGELOG.txt) to find credits for translators at each release!
