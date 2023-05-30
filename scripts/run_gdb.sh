PD_VER=0.53-0
PD_BIN=/Applications/Studio/Pd-${PD_VER}.app/Contents/Resources/bin/pd

cd chuck~ && gdb --args ${PD_BIN} -path . -nogui -send "pd dsp 1" -stderr help-chuck.pd
