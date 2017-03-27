@echo off

set CommonCompilerFlags=-MTd -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4211 -wd4100 -wd4189 -wd4505 -wd4244 -wd4267 -wd4702 -DBUS_INTERNAL=1 -DBUS_SLOW=1 -DBUS_WIN32=1 -FC -Z7 /I "f:\dev\bus\code\SFML-2.4.2\include" /EHsc
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib sfml-graphics-d.lib sfml-window-d.lib sfml-system-d.lib sfml-audio-d.lib

REM TODO - can we just build both with one exe?

IF NOT EXIST ..\build mkdir ..\build
pushd ..\build

REM 32-bit build
REM cl %CommonCompilerFlags% ..\code\win32_bus.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags%

REM 64-bit build
del *.pdb > NUL 2> NUL
REM Optimization Switches /O2 /Oi /fp:fast
set PDBNAME=bus_%random%.pdb
set PDBNAME=%PDBNAME: =0%
cl %CommonCompilerFlags% ..\code\bus.cpp -Fmwnetrunner.map /LD /link /libpath:f:\dev\bus\code\SFML-2.4.2\lib -incremental:no -PDB:%PDBNAME% /DLL /EXPORT:GameUpdateAndRender /EXPORT:GameGetSoundSamples %CommonLinkerFlags%
cl %CommonCompilerFlags% ..\code\win32_bus.cpp -Fmwin32_bus.map /link /libpath:f:\dev\bus\code\SFML-2.4.2\lib %CommonLinkerFlags%
popd
