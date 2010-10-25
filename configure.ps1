
#param([parameter(ValueFromRemainingArguments=$true)][String[]]$files, `
#[alias("l")][String][ValidateSet("0", "1", "2", "3", "4")]$log_level = 1)
#Write-Host "Switch (-l): $log_level"
#foreach ($x in $files) {
#Write-Host "Extra arg: $x"
#}

param ( 
        [string] $g = "NMake Makefiles"
	  , [string] $generator
	  , [string] $f
	  , [string] $folder_name
	  , [string] $folder_path
	  , [string] $b
	  , [string] $build = "release"
	  , [string] $d
	  , [string] $documentation
	  , [string] $p
	  , [string] $param
	  , [string] $e
	  ,	[string] $extern_path
	  , [string[]] $cmake_param
	  , [switch] $t
	  , [switch] $enable_testing
	  , [string] $test_name
	  , [string] $test_site
	  , [switch] $h
	  ,	[switch] $help
)

if($g)
{
	$generator = $g
}
if($f)
{
	$folder_name = $f
}
if($t)
{
	$enable_testing = $t
}
if($d)
{
	$documentation = $d
}

if($e)
{
	$extern_path = $e
}
function printHelp()
{
	Write-Host
	Write-Host "Usage:"
	Write-Host ".\configure.ps1 [-g|-generator GeneratorName] [-f|-folder FolderName] [-b|-build BuildType]"
	Write-Host " [-p|-param ParamName] [-d|-documentation full/fast] [-h|-help]"
	Write-Host " [-E|-extern-path ExternAbsPath]"
	Write-Host " [-t|-enable-testing] [-test-name name] [-test-site addr]"
	Write-Host " [-P|-cmake-param CMakeParameter]"
	Write-Host
	Write-Host "Options:"
	Write-Host "[-g|-generator GeneratorName]: Specify the cmake generator - default cmake's default"
	Write-Host "[-f|-folder-name FolderName]: Specify the build folder name - default is the generator name or the build type if the generator was not specified"
	Write-Host "[-F|-folder-path FolderPath]: Specify the build folder path - unlike the build folder-name wich creates a folder under build, this flag allows creating a build folder anywhere"
	Write-Host "[-b|-build BuildType]: The cmake build type release/debug/nolog/maintain - default release"
	Write-Host "[-p|-param ParamName]: Extra compilation flags. E.g. -p `"-DUINDEX64 -DUSERVICEBITS=3`""
	Write-Host "[-d|-documentation full/fast]: Build either fast or full documentation"
	Write-Host "[-e|-extern-path ExternAbsPath]: absolute path to extern libraries"
	Write-Host "[-t|-enable-testing]: Enable unit-testing using ctest"
	Write-Host "[-test-name name]: The name of ctest/cdash project"
	Write-Host "[-test-site addr]: The address for cdash drop site"
	Write-Host "[-P|-cmake-param]: Some parameters given to cmake like: `"-DECLIPSE_CDT4_GENERATE_SOURCE_PROJECT=TRUE`""
	Write-Host
	Write-Host "Examples:"
	Write-Host "1) create simple make release build:"
	Write-Host "	.\configure.ps1 -f rls -b release -e d:/work/sg_extern"
	Write-Host "2) create a release build but give the parameters as cmake parameters"
	Write-Host "	.\configure.ps1 -f rel -b release -cmake_param `"-DUDEFS:STRING=`'-DUDEBUG -DUASSERT`'`",`"-DUEXTERN_ABS:STRING=`'D:/work/als_extern/release`'`""
	Write-Host
	Write-Host "3) create a visual studio 2008 solution:"
	Write-Host "    .\configure.ps1 -f vs9 -b debug -e `"D:\work\sg_extern`" -g `"Visual Studio 9 2008`""
	Write-Host
	Write-Host "Note:`nPlease consider running `"cmake -h`" to get the list of available generators"
	Write-Host
}

function make_cmake_list()
{
	$files=get-childitem -name
	Write-Host $files
	if($files.Length -ge 1)
	{		
		$f1 = New-Item -type file CMakeLists.txt -Force
		foreach($file in $files)
		{
			add-content $f1 "add_subdirectory(`"$file`")"
		}
	}else{
		$f1 = New-Item -type file CMakeLists.txt -Force
		add-content $f1 ""
	}
}

if($h -or $help){
	printHelp
	exit
}

if($documentation -eq "full")
{
	Write-Host "Building full documentation ..."
	rm documentation\html\
	rm documentation\latex\
	#doxygen
	#tar -cjf documentation/full.tar.bz2 documentation/html/ documentation/index.html
	Write-Host "Done building full documentation"
	exit
}

if($documentation -eq "fast")
{
	Write-Host "Building fast documentation ..."
	rm documentation\html\
	rm documentation\latex\
	#doxygen Doxyfile.fast
	#tar -cjf documentation/full.tar.bz2 documentation/html/ documentation/index.html
	Write-Host "Done building full documentation"
	exit
}

$current_folder = (Get-Location -PSProvider FileSystem).ProviderPath

Write-Host "Current folder $current_folder"

cd application
if(test-path "CMakeLists.txt")
{
rm CMakeLists.txt -force
}
make_cmake_list
cd ../

cd library
if(test-path "CMakeLists.txt")
{
rm CMakeLists.txt -force
}
make_cmake_list
cd ../

if(!($folder_name))
{
	if($generator)
	{
		$folder_name = $generator
	}
	else
	{
		$folder_name = $build
	}
}

if(!($folder_path -eq ""))
{
	if(test-path $folder_path)
	{
		rm -force $folder_path
	}
	mkdir $folder_path
	cd $folder_path
}
else
{
	if(!(test-path "build"))
	{
		mkdir "build"
	}
	cd build
	if(!(test-path "$folder_name"))
	{
		mkdir "$folder_name"
	}
	cd "$folder_name"
}

foreach($cp in $cmake_param)
{
	Write-Host "$cp"
}
if(!($generator -eq ""))
{
	cmake -G $generator -DCMAKE_BUILD_TYPE=$builf "-DUDEFS:STRING=`"$param`"" "-DUEXTERN_ABS:STRING=`"$extern_path`"" "-DUTESTING:STRING=`"$enable_testing`"" "-DUTEST_NAME=`"$test_name`"" "-DUTEST_SITE=`"$test_site`"" $cmake_param "$current_folder"
}
else
{
	cmake -DCMAKE_BUILD_TYPE=$builf "-DUDEFS:STRING=`"$param`"" "-DUEXTERN_ABS:STRING=`"$extern_path`"" "-DUTESTING:STRING=`"$enable_testing`"" "-DUTEST_NAME=`"$test_name`"" "-DUTEST_SITE=`"$test_site`"" $cmake_param "$current_folder"
}
cd "$current_folder"
