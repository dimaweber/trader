TEMPLATE=subdirs

SUBDIRS += common \
           daemon \
           db_check \
           infobot \
           tgbot-cpp

daemon.depends = common
infobot.depends = common tgbot-cpp
db_check.depends = common

OTHER_FILES += \
               sql/deposits_for_every_day.sql \
               sql/db_upgrade_v1.0.sql \
               sql/db_upgrade_v2.0.sql \
               sql/create_db_v1.0.sql

DISTFILES += \
    sql/db_upgrade_v2.1.sql \
    sql/db_upgrade_v2.2.sql \
    sql/sanity_check_v2.3.sql

