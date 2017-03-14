TEMPLATE=subdirs

SUBDIRS += common \
           daemon \
           db_check \
           infobot \
           tgbot-cpp

daemon.depends = common
infobot.depends = common tgbot-cpp
db_check.depends = common


#TODO!!!!!!!!: ini file with db settings, common for all executables!!!!!
