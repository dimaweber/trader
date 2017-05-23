#ifndef TYPES_CPP
#define TYPES_CPP

#include "types.h"

#include <QDataStream>

template <int n>
QDataStream& operator << (QDataStream& stream, const DEC_NAMESPACE::decimal<n>& d)
{
    stream << dec2qstr(d, n);
    return stream;
}

template <int n>
QDataStream& operator >> (QDataStream& stream, DEC_NAMESPACE::decimal<n>& d)
{
    QString str;
    stream >> str;
    d = qstr2dec<n>(str);
    return stream;
}

QByteArray PairInfo::pack() const
{
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream << min_price
           << max_price
           << min_amount
           << fee
           << pair_id
           << decimal_places
           << hidden
           << pair;
    return buffer;
}

#endif // TYPES_CPP

bool PairInfo::unpack(QByteArray &ba)
{
    QDataStream stream(&ba, QIODevice::ReadOnly);
    stream >> min_price
            >> max_price
            >> min_amount
            >> fee
            >> pair_id
            >> decimal_places
            >> hidden
            >> pair;
    return true;
}

QByteArray TickerInfo::pack() const
{
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream << high
           << low
           << avg
           << vol
           << vol_cur
           << buy
           << sell
           << last
           << updated
           << pairName;
    return buffer;
}

bool TickerInfo::unpack(QByteArray &ba)
{
    QDataStream stream(&ba, QIODevice::ReadOnly);
    stream >> high
            >> low
            >> avg
            >> vol
            >> vol_cur
            >> buy
            >> sell
            >> last
            >> updated
            >> pairName;
    return true;
}

QByteArray OrderInfo::pack() const
{
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream << pair
           << static_cast<int>(type)
           << start_amount
           << amount
           << rate
           << created
           << static_cast<int>(status)
           << user_id
           << order_id;
    return buffer;
}

bool OrderInfo::unpack(QByteArray &ba)
{
    QDataStream stream(&ba, QIODevice::ReadOnly);
    int t;
    int s;
    stream >> pair
           >> t
           >> start_amount
           >> amount
           >> rate
           >> created
           >> s
           >> user_id
           >> order_id;
    type = static_cast<OrderInfo::Type>(t);
    status = static_cast<OrderInfo::Status>(s);
    return true;
}

QByteArray UserInfo::pack() const
{
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream << user_id
           << name
           << funds;
    return buffer;
}

bool UserInfo::unpack(QByteArray &ba)
{
    QDataStream stream(&ba, QIODevice::ReadOnly);
    stream >> user_id
           >> name
           >> funds;
    return true;
}

QByteArray ApikeyInfo::pack() const
{
    QByteArray buffer;
    QDataStream stream(&buffer, QIODevice::WriteOnly);
    stream << apikey
           << info
           << trade
           << withdraw
           << secret
           << nonce
           << user_id;
    return buffer;
}

bool ApikeyInfo::unpack(QByteArray &ba)
{
    QDataStream stream(&ba, QIODevice::ReadOnly);
    stream >> apikey
           >> info
           >> trade
           >> withdraw
           >> secret
           >> nonce
           >> user_id;
    return true;
}
