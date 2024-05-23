set VERSION=2.72.14

IF NOT EXIST Faust-%VERSION%-win64.exe (
    curl -L https://github.com/grame-cncm/faust/releases/download/%VERSION%/Faust-%VERSION%-win64.exe -o Faust-%VERSION%-win64.exe
    call Faust-%VERSION%-win64.exe /S /D=%cd%\win64\Release
)