# This file patches the chuck `core` and `host` folders to eliminate naming collisions
# The script requires the [rpl](https://pypi.org/project/rpl/) 
# search and replace utility which can be installed via: `pip install rpl`

TARGET=thirdparty/chuck

RPL="rpl --match-case \
		 --whole-words \
		 --verbose \
		 --fixed-strings \
		 --recursive \
		 --glob *.cpp \
		 --glob *.c \
		 --glob *.h \
		 --glob *.hpp"


${RPL} t_array t_carray   ${TARGET}
${RPL} t_class t_cclass   ${TARGET}
${RPL} mtof    ck_mtof    ${TARGET}
${RPL} ftom    ck_ftom    ${TARGET}
${RPL} rmstodb ck_rmstodb ${TARGET}
${RPL} powtodb ck_powtodb ${TARGET}
${RPL} dbtopow ck_dbtopow ${TARGET}
${RPL} dbtorms ck_dbtorms ${TARGET}
