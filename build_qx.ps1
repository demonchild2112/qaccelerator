$sevenzip = 'C:\Program Files\7-Zip\7z.exe'
$binarycreator = 'C:\Qt\Tools\QtInstallerFramework\2.0\bin\binarycreator.exe'
$env:Path += ';C:\Qt\5.9.1\msvc2017_64\bin'
$env:Path += ';C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\amd64'
$env:Path += ';C:\Program Files (x86)\Windows Kits\10\bin\x64'
cd C:\projects\qaccelerator
qmake
nmake
ls release
ls 'C:\Program Files (x86)\Windows Kits'
