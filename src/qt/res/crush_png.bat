@ECHO OFF
setlocal enabledelayedexpansion
for %%f in (C:\cloakcoin\cpa3\src\qt\res\icons\*.png) do (
  pngcrush -ow -rem allb -reduce %%f
)

for %%f in (C:\cloakcoin\cpa3\src\qt\res\images\*.png) do (
  pngcrush -ow -rem allb -reduce %%f
)