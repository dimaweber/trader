#ifndef TYPES_H
#define TYPES_H

#include "decimal.h"
#include "qglobal.h"

#include <QDateTime>
#include <QMap>
#include <QMutex>
#include <QString>
#include <QVariant>

#include <memory>

using Amount = DEC_NAMESPACE::decimal<7>;
using Fee    = DEC_NAMESPACE::decimal<7>;
using Rate   = DEC_NAMESPACE::decimal<7>;
using PairId = quint32;
using UserId = quint32;
using TradeId = quint32;
using OrderId = quint32;
using PairName = QString;
using ApiKey = QString;

template <int n>
QString dec2qstr(const DEC_NAMESPACE::decimal<n>& d, int decimal_places =7)
{
    switch (decimal_places) {
        case 0: return QString::fromStdString(DEC_NAMESPACE::toString(DEC_NAMESPACE::decimal_cast<0>(d)));
        case 1: return QString::fromStdString(DEC_NAMESPACE::toString(DEC_NAMESPACE::decimal_cast<1>(d)));
        case 2: return QString::fromStdString(DEC_NAMESPACE::toString(DEC_NAMESPACE::decimal_cast<2>(d)));
        case 3: return QString::fromStdString(DEC_NAMESPACE::toString(DEC_NAMESPACE::decimal_cast<3>(d)));
        case 4: return QString::fromStdString(DEC_NAMESPACE::toString(DEC_NAMESPACE::decimal_cast<4>(d)));
        case 5: return QString::fromStdString(DEC_NAMESPACE::toString(DEC_NAMESPACE::decimal_cast<5>(d)));
        case 6: return QString::fromStdString(DEC_NAMESPACE::toString(DEC_NAMESPACE::decimal_cast<6>(d)));
        case 7:
        default: return QString::fromStdString(DEC_NAMESPACE::toString(DEC_NAMESPACE::decimal_cast<7>(d)));
    }
}

template <int n>
DEC_NAMESPACE::decimal<n> qstr2dec(const QString& s)
{
    return DEC_NAMESPACE::decimal<n>(s.toStdString());
}

template <int n>
DEC_NAMESPACE::decimal<n> qvar2dec(const QVariant& s)
{
    return DEC_NAMESPACE::decimal<n>(s.toDouble());
}

enum Method {Invalid, AuthIssue, AccessIssue,
             PublicInfo, PublicTicker, PublicDepth, PublicTrades,
             PrivateGetInfo, PrivateTrade, PrivateActiveOrders, PrivateOrderInfo,
             PrivateCanelOrder, PrivateTradeHistory, PrivateTransHistory,
             PrivateCoinDepositAddress,
             PrivateWithdrawCoin, PrivateCreateCupon, PrivateRedeemCupon
            };

struct PairInfo
{
    using Ptr = std::shared_ptr<PairInfo>;
    using WPtr = std::weak_ptr<PairInfo>;
    using List = QList<PairInfo::Ptr>; // WPtr ?

    Rate min_price;
    Rate max_price;
    Rate min_amount;
    Fee fee;
    PairId pair_id;
    int decimal_places;
    bool hidden;
    PairName pair;

    QByteArray pack() const;
    bool unpack(QByteArray& ba);
};
struct TickerInfo
{
    using Ptr = std::shared_ptr<TickerInfo>;

    Rate high;
    Rate low;
    Rate avg;
    Amount vol;
    Amount vol_cur;
    Rate last;
    Rate buy;
    Rate sell;
    QDateTime updated;
    PairName pairName;

    PairInfo::WPtr pair_ptr;

    QMutex updateAccess;

    QByteArray pack() const;
    bool unpack(QByteArray& ba);
};
struct OrderInfo
{
    enum class Type {Sell, Buy};
    enum class Status {Active=1, Done, Cancelled, PartiallyDone};
    using Ptr = std::shared_ptr<OrderInfo>;
    using WPtr = std::shared_ptr<OrderInfo>;
    using List = QList<OrderInfo::Ptr>; // WPtr ?

    PairName pair;
    std::weak_ptr<PairInfo> pair_ptr;
    Type type;
    Amount start_amount;
    Amount amount;
    Rate rate;
    QDateTime created;
    Status status;
    UserId user_id;
    OrderId order_id;

    QMutex updateAccess;

    QByteArray pack() const;
    bool unpack(QByteArray& ba);
};
struct TradeInfo
{
    enum class Type {Ask, Bid};
    using Ptr = std::shared_ptr<TradeInfo>;
    using List  = QList<TradeInfo::Ptr>; // Wptr ?

    TradeInfo::Type type;
    Rate rate;
    Amount amount;
    TradeId tid;
    QDateTime created;
    OrderId order_id;
    UserId user_id;
    OrderInfo::WPtr order_ptr;
};

using DepthItem = QPair<Rate, Amount>;
using Depth = QList<DepthItem>;
using BuySellDepth = QPair<Depth, Depth>;
using Funds = QMap<QString, Amount>;

struct UserInfo
{
    using Ptr = std::shared_ptr<UserInfo>;
    using WPtr = std::weak_ptr<UserInfo>;

    UserId user_id;
    QString name;
    Funds funds;

    QMutex updateAccess;

    QByteArray pack() const;
    bool unpack(QByteArray& ba);
};

struct ApikeyInfo
{
    using Ptr = std::shared_ptr<ApikeyInfo>;

    ApiKey apikey;
    bool info;
    bool trade;
    bool withdraw;
    QByteArray secret;
    quint32 nonce;
    UserId user_id;
    UserInfo::WPtr user_ptr;

    QMutex updateAccess;

    QByteArray pack() const;
    bool unpack(QByteArray& ba);
};

#endif // TYPES_H
