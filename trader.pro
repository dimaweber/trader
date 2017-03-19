TEMPLATE=subdirs

SUBDIRS += common \
           btce \
database \
           daemon \
           db_check \
           infobot \
           tgbot-cpp

btce.depends = common
database.depends = common
daemon.depends = common btce database
infobot.depends = common tgbot-cpp
db_check.depends = common database

OTHER_FILES += \
               sql/deposits_for_every_day.sql \
               sql/create_db_v1.0.sql

