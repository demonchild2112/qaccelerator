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

$env:PATH += ';C:\Qt\5.9.1\msvc2015_64\bin'
# $env:PATH += ';C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\amd64'

qmake
nmake
ls release
