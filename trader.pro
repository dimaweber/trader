TEMPLATE=subdirs

SUBDIRS += common \
           btce \
           database \
           mailer \
           daemon \
           db_check \
           add_order \
    bitfinex_client \
    tests

!win32:!linux-* {
SUBDIRS += tgbot-cpp \
           infobot \
           emul \
           emulatorClients

infobot.depends = tgbot-cpp database
emul.depends = database btce
emulatorClients.depends=emul
}

btce.depends = common
database.depends = common
daemon.depends =  btce database
db_check.depends =  database
add_order.depends = database btce
bitfinex_client.depends = btce
tests.depends = btce

OTHER_FILES += \
               sql/deposits_for_every_day.sql \
               sql/create_db_v1.0.sql

