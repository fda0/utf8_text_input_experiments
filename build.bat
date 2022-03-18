@echo off

set BaseFile="App.cpp"
set MsvcLinkFlags=-incremental:no -opt:ref -machine:x64 -manifest:no
set MsvcCompileFlags=-Zi -Zo -Gy -GF -GR- -EHs- -EHc- -EHa- -WX -W4 -nologo -FC -diagnostics:column -fp:except- -fp:fast -wd4100 -wd4189 -wd4201 -wd4505 -wd4996

echo -----------------
echo ---- Building debug:
call cl -FeApp_debug_msvc.exe -Od %MsvcCompileFlags% %BaseFile% /link %MsvcLinkFlags% -RELEASE

echo -----------------
echo ---- Building release:
call cl -FeApp_release_msvc.exe -Oi -Oxb2 -O2 %CLCompileFlags% %BaseFile% /link %CLLinkFlags% -RELEASE