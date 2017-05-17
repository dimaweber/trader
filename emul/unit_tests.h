#ifndef UNIT_TESTS_H
#define UNIT_TESTS_H

#include "authentificator.h"
#include "responce.h"
#include "sqlclient.h"

#include <QtTest>

// TODO: add own sql client for tests s they can check consistency json to real data

class QSqlDatabase;

class BtceEmulator_Test : public QObject
{
    Q_OBJECT
    quint32 nonce();
    std::unique_ptr<Responce> client;
    std::unique_ptr<SqlClient> sqlClient;
public:
    BtceEmulator_Test(QSqlDatabase& db);
private slots:
    void FcgiRequest_httpGetQuery();
    void FcgiRequest_httpPostQuery();

    void QueryParser_urlParse();
    void QueryParser_methodNameRetrieving();
    void QueryParser_duplicatingPairsRetrieving();
    void QueryParser_pairsRetrieving();
    void QueryParser_limitParameterRetrieving();
    void QueryParser_defaultLimitValue();
    void QueryParser_limitOverflow();

    void Methods_invalidMethod();
    void Methods_publicInfo();
    void Methods_publicTicker();
    void Methods_publicDepth();
    void Methods_publicTrades();

    void Info_serverTimePresent();
    void Info_pairsPresent();

    void Ticker_emptyList();
    void Ticker_emptyList_withIgnore();
    void Ticker_emptyPairnameHandle();
    void Ticker_onePair();
    void Ticker_twoPairs();
    void Ticker_ignoreInvalid();
    void Ticker_ignoreInvalidWithouValue();
    void Ticker_ignoreInvalidInvalidSetToZero();
    void Ticker_invalidPair();
    void Ticker_InvalidPairPrevailOnDoublePair();
    void Ticker_reversePairName();
    void Ticker_timestampFormat();
    void Ticker_duplicatePair();
    void Ticker_ignoreInvalidForDuplicatePair();

    void Authentication_noKey();
    void Authentication_invalidKey();
    void Authentication_noSign();
    void Authentication_invalidSign();
    void Authentication_infoPermissionCheck();
    void Authentication_tradePermissionCheck();
    void Authentication_withdrawPermissionCheck();
    void Authentication_noNonce();
    void Authentication_invalidNonce();

    void Method_privateGetInfo();
    void Method_privateActiveOrders();
    void Method_privateOrderInfo();
    void Method_privateTrade();
    void Method_privateCancelOrder();

    void Depth_emptyList();
    void Depth_valid();
    void Depth_twoPairs();
    void Depth_sortedByRate();
    void Depth_ratesDecimalDigits();
    void Depth_limit();

    void Trades_emptyList();
    void Trades_valid();
    void Trades_limit();
    void Trades_sortedByTimestamp();

    void GetInfo_valid();

    void ActiveOrders_valid();

    void OrderInfo_missingOrderId();
    void OrderInfo_wrongOrderId();
    void OrderInfo_valid();

    void Trade_parameterPairPresenceCheck();
    void Trade_parameterPairValidityCheck();
    void Trade_parameterTypeCheck();
    void Trade_parameterAmountMinValueCheck();
    void Trade_parameterRateMinValueCheck();
    void Trade_buy();
    void Trade_sell();
    void Trade_balanceValid();
    void Trade_tradeBenchmark();
};
#endif // UNIT_TESTS_H
