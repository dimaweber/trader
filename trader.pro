TEMPLATE=subdirs

SUBDIRS += common \
		   daemon \
		   db_check \
		   infobot \
		   tgbot-cpp

daemon.depends = common
infobot.depends = common tgbot-cpp
db_check.depends = common

OTHER_FILES += sql/sanity_check.sql
#TODO!!!!!!!!: ini file with db settings, common for all executables!!!!!
