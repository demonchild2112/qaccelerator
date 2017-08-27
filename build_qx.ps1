$sevenzip = 'C:\Program Files\7-Zip\7z.exe'
$binarycreator = 'C:\Qt\Tools\QtInstallerFramework\2.0\bin\binarycreator.exe'
cd C:\projects\qaccelerator

# Prepare the build environment, and dump the vars to files so
# we can load them here.
& C:\projects\qaccelerator\prepare_build_env.bat
$env:PATH = cat env.path.txt
$env:LIB = cat env.lib.txt
$env:LIBPATH = cat env.libpath.txt
$env:INCLUDE = cat env.include.txt

qmake
nmake
ls release
