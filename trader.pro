TEMPLATE=subdirs

SUBDIRS += common \
		   daemon \
#           infobot

daemon.depends = common
#infobot.depends = common
