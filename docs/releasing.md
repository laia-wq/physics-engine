# Release Guide

This document is for project maintainers preparing a downloadable release. Most
users should download the finished application or follow the source-build
instructions in the main README.

## Build the macOS package

On an Apple-silicon Mac with Homebrew, CMake, and SFML installed:

```sh
./scripts/package-macos.sh
```

The script:

1. Creates a clean Release build.
2. Runs all automated tests.
3. Constructs a standard macOS `.app` bundle.
4. Embeds SFML, FreeType, and PNG libraries.
5. Rewrites dynamic-library paths to use the bundled copies.
6. Applies an ad-hoc signature.
7. Creates `dist/Particle-Physics-Laboratory-macOS-arm64.zip`.

Before publishing, open the packaged application and perform the release-candidate
checks. Upload the ZIP—not the uncompressed application—as a GitHub Release asset.

The app is not notarized with a paid Apple Developer certificate. Document that
users may need to right-click the app and choose **Open** on first launch.
