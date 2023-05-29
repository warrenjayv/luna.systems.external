@echo off
echo: 
xcopy luna.external.console.cs build /-y
echo:
xcopy luna.external.console.writer.cs build /-y

cd build
dotnet publish --configuration Release
     
