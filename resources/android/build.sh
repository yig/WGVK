#! /usr/bin/bash

# === 1. Clean previous build artifacts (IMPORTANT!) ===
# This removes the old arm64 libraries and build intermediates.
rm -rf build obj libs
ndk-build clean
echo "Cleaned previous build."


# === 2. Build the new x86_64 native library ===
ndk-build
echo "Built native library for x86_64."


# === 3. Compile and Link Android Resources ===
# Create directories for build output
mkdir -p build/gen
# Compile resources with aapt2
aapt2 compile --dir res -o build/res.zip
# Link resources, generate R.java, and create the base APK
aapt2 link -o build/unsigned.apk \
    -I $ANDROID_HOME/platforms/android-34/android.jar \
    --manifest AndroidManifest.xml \
    -R build/res.zip \
    --java build/gen \
    --auto-add-overlay
echo "Compiled and linked resources."


# === 4. Compile Java Code (targeting Java 17) ===
mkdir -p build/classes
javac --release 17 -d build/classes \
    -classpath $ANDROID_HOME/platforms/android-34/android.jar \
    -sourcepath src:build/gen \
    src/com/example/minimal/MainActivity.java
echo "Compiled Java code."


# === 5. Convert Java bytecode to DEX format ===
d8 build/classes/com/example/minimal/*.class --output build --lib $ANDROID_HOME/platforms/android-34/android.jar
echo "Created DEX file."


# === 6. Package everything into the APK ===
# Add the DEX file
zip -uj build/unsigned.apk build/classes.dex

# Add the native library with the correct internal path (lib/x86_64/)
# The 'mv' trick is the cleanest way to ensure the directory structure is right.
mv libs lib
zip -r build/unsigned.apk lib
mv lib libs
echo "Packaged DEX and native library into APK."


# === 7. Sign the Final APK ===
apksigner sign --ks debug.keystore --ks-key-alias androiddebugkey --ks-pass pass:android build/unsigned.apk
mv build/unsigned.apk minimal.apk
echo "Signed APK: minimal.apk is ready to install."
