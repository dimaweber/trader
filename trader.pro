TEMPLATE=subdirs

SUBDIRS += common \
           btce \
           database \
           mailer \
           tgbot-cpp \
           daemon \
           db_check \
           infobot \
           add_order \
    bitfinex_client


btce.depends = common
database.depends = common
daemon.depends =  btce database
infobot.depends = tgbot-cpp database
db_check.depends =  database
add_order.depends = database btce

OTHER_FILES += \
               sql/deposits_for_every_day.sql \
               sql/create_db_v1.0.sql

