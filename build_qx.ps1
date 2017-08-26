$sevenzip = 'C:\Program Files\7-Zip\7z.exe'
$binarycreator = 'C:\Qt\Tools\QtInstallerFramework\2.0\bin\binarycreator.exe'
$qmake = 'C:\Qt\5.9.1\msvc2017_64\bin\qmake.exe'
$nmake = 'C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\amd64\nmake.exe'
cd C:\projects\qaccelerator
$qmake
$nmake
ls release
