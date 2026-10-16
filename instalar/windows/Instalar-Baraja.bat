@echo off
setlocal
rem Instala Baraja en Windows: plugin VST3 y CLAP para todos los usuarios,
rem y la aplicacion independiente en la carpeta de programas del usuario.

rem Copiar a "Archivos de programa" necesita permisos de administrador.
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Pidiendo permisos de administrador...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

cd /d "%~dp0"

if not exist "Baraja.vst3" (
    echo No encuentro Baraja.vst3 junto a este instalador.
    echo Descomprimi el paquete completo antes de ejecutarlo.
    pause
    exit /b 1
)

echo.
echo Instalando Baraja...

if exist "%CommonProgramFiles%\VST3\Baraja.vst3" rmdir /s /q "%CommonProgramFiles%\VST3\Baraja.vst3"
xcopy /E /I /Y /Q "Baraja.vst3" "%CommonProgramFiles%\VST3\Baraja.vst3" >nul
echo   OK  VST3  -^> %CommonProgramFiles%\VST3

if not exist "%CommonProgramFiles%\CLAP" mkdir "%CommonProgramFiles%\CLAP"
copy /Y "Baraja.clap" "%CommonProgramFiles%\CLAP\" >nul
echo   OK  CLAP  -^> %CommonProgramFiles%\CLAP

if not exist "%LOCALAPPDATA%\Programs\Baraja" mkdir "%LOCALAPPDATA%\Programs\Baraja"
copy /Y "Baraja.exe" "%LOCALAPPDATA%\Programs\Baraja\" >nul
powershell -NoProfile -Command "$s=(New-Object -ComObject WScript.Shell).CreateShortcut([Environment]::GetFolderPath('Programs') + '\Baraja.lnk'); $s.TargetPath='%LOCALAPPDATA%\Programs\Baraja\Baraja.exe'; $s.Save()"
echo   OK  Aplicacion independiente (en el menu Inicio como "Baraja")

echo.
echo Listo. En Reaper: Options - Preferences - Plug-ins - VST - "Re-scan".
echo.
pause
