TEMPLATE=subdirs

SUBDIRS += common \
           daemon \
           db_check \
           infobot \
           tgbot-cpp

daemon.depends = common
infobot.depends = common tgbot-cpp
db_check.depends = common
