TEMPLATE=subdirs

SUBDIRS += daemon infobot common

daemon.depends = common
infobot.depends = common
