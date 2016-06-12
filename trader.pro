TEMPLATE=subdirs

SUBDIRS += daemon infobot common tgbot-cpp

daemon.depends = common
infobot.depends = common tgbot-cpp
