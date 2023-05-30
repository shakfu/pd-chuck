# This file patches the chuck `core` and `host` folders to eliminate naming collisions
# The script requires the [rpl](https://pypi.org/project/rpl/) 
# search and replace utility which can be installed via: `pip install rpl`


RPL="rpl --match-case \
		 --whole-words \
		 --verbose \
		 --fixed-strings \
		 --recursive \
		 --glob *.cpp \
		 --glob *.c \
		 --glob *.h \
		 --glob *.hpp"


${RPL} t_array t_carray .
${RPL} t_class t_cclass .
${RPL} mtof    ck_mtof .
${RPL} ftom    ck_ftom .
${RPL} rmstodb ck_rmstodb .
${RPL} powtodb ck_powtodb .
${RPL} dbtopow ck_dbtopow .
${RPL} dbtorms ck_dbtorms .
