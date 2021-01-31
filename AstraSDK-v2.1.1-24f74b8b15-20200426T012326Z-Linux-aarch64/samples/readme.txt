build samples on windows
========================
  If you use vs2013 or vs2015, download the zip file for vs2013,
you can find astra-samples.sln in samples/vs2013.
  If you use vs2015 or higher version, download the zip file for
vs2015, you can find astra-samples.sln in samples/vs2015.
  Another way is generating astra-samples.sln from CMakeLists.txt
by youself. In this folder, run "cmake ." with '-G' to set visual
studio version and '-A' to set platform.

build samples on linux
========================
  Before building you should install 'cmake', 'libsfml-dev' and
'pkg-config'.
  Then in this folder, run 'cmake .' and 'make install'. The
samples will be built in bin/ folder.
