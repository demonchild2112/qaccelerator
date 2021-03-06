﻿$ErrorActionPreference = "Stop"

if ($args.Count -eq 0 -or ($args[0] -ne 'first_stage' -and $args[0] -ne 'installer')) {
  Write-Error -Message 'Usage: build_qx [first_stage|installer]'
  exit 1
}

$BASE_DIR = (Get-Location).Path
$TMP_DIR = $BASE_DIR + '\tmp'
$compiler_dir = (Get-ChildItem C:\Qt\*\msvc20??_64\bin)[-1].FullName
Write-Information "Using compiler: $compiler_dir"

if ($args[0] -eq 'first_stage') {
  if ([System.IO.Directory]::Exists($TMP_DIR)) {
    Write-Information 'Deleting existing work dir'
    Remove-Item -Recurse -Force $TMP_DIR
  }
  mkdir $TMP_DIR
  cd $TMP_DIR
  & ..\prepare_build_env.bat
  $env:PATH = cat env.path.txt
  $env:LIB = cat env.lib.txt
  $env:LIBPATH = cat env.libpath.txt
  $env:INCLUDE = cat env.include.txt
  $env:PATH += ";$compiler_dir"

  qmake ../

  if (!$?) {
    exit 2
  }

  nmake

  if (!$?) {
    exit 3
  }

  cd $BASE_DIR
  exit 0
}

if (![System.IO.Directory]::Exists($TMP_DIR)) {
  Write-Error -Message "Temp build directory not found: $TMP_DIR"
  exit 4
}

$release_binary = $TMP_DIR + '\release\qaccelerator.exe'
if (![System.IO.File]::Exists($release_binary)) {
  Write-Error -Message "Release binary not found: $release_binary"
  exit 5
}

cd $TMP_DIR

Get-ChildItem . -Exclude 'release' | Remove-Item -Recurse -Force

$svnz_installdir = Get-ItemPropertyValue 'HKLM:\SOFTWARE\7-Zip' 'Path'
$svnz = $svnz_installdir + '7z.exe'
Write-Information "Using 7-zip binary:  $svnz"

$windeploy = $compiler_dir + '\windeployqt.exe'
Write-Information "Using windeploy binary: $windeploy"

$qif_versions = Get-ChildItem C:\Qt\Tools\QtInstallerFramework | Where-Object {$_.Name -match '\d'}
$binarycreator = $qif_versions[-1].FullName + '\bin\binarycreator.exe'
Write-Information "Using binarycreator binary: $binarycreator"

mkdir deps_dir
cd deps_dir
cp $release_binary .
& $windeploy -core -gui -sql -network qaccelerator.exe
& $svnz a QAccelerator_x64.7z
if (!$?) {
  exit 6
}
cd $TMP_DIR

mkdir -p packages\core\data
cp -Recurse $BASE_DIR\build\config .
cp -Recurse $BASE_DIR\build\meta packages\core
cp deps_dir\QAccelerator_x64.7z packages\core\data

$version_file = Get-Content ($BASE_DIR + '\version.h')
$version_match = $version_file | Select-String -Pattern 'VERSION = "([\d|\.]+)";'
$version = $version_match.Matches.Groups[1].Value

$config_path = $TMP_DIR + '\config\config.xml'
$config = [xml](Get-Content $config_path)
$version_elt = $config.CreateElement('Version')
$version_elt.set_InnerXML($version)
$config.Installer.AppendChild($version_elt)
$config.Save($config_path)

$package_path = $TMP_DIR + '\packages\core\meta\package.xml'
$package = [xml](Get-Content $package_path)
$version_elt = $package.CreateElement('Version')
$version_elt.set_InnerXML($version)
$package.Package.AppendChild($version_elt)
$date_elt = $package.CreateElement('ReleaseDate')
$today = Get-Date -UFormat '%Y-%m-%d'
$date_elt.set_InnerXML($today)
$package.Package.AppendChild($date_elt)
$package.Save($package_path)

& $binarycreator -c $config_path -p packages QAccelerator_x64_Setup.exe

if (!$?) {
  exit 7
}

& $svnz a -tzip QAccelerator_x64_Setup.zip QAccelerator_x64_Setup.exe

cd $BASE_DIR
