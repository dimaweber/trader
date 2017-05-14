#!/bin/sh

METHOD=CancelOrder
#PARAMS=`date +method=$METHOD\&nonce=%s\&amount=0.02\&rate=1588\&pair=btc_eur\&type=sell`
PARAMS=`date +method=$METHOD\&nonce=%s\&order_id=76656`

KEY="2RG2YQX3-R5K2OFWG-3PSHUMC6-3M4O5A4T-LGRV3WLA"
SECRET="110jxiqndrz5zciuu0d53isz1dioy7qr2so71nn5waui9vnt3lfawk3o5rrvas2d"

#KEY='YGE6F4M3-DBDYOSO1-GXV9D3SN-SENXSSOG-ZOLCB33G'
#SECRET='wvnbmbsgxtzy4rrz9frhmqh6yzz0j7rh9cbsqxgz1p6ki4cf7705q6lptawz3bhh'

#btce key
#KEY="2S1XKFM6-NYO3X5FH-QGHGE9IT-GLL3UEL7-QA26ESCI"
#SECRET="db1a085f7c9df4b6e235a6f0d56cb6099ba5c3fe60a799ba130d6fcb34801715"

HOST="localhost:81"
SCHEMA="http"

SIGN=`echo  -n "$PARAMS" | openssl dgst -sha512 -hmac $SECRET | sed "s/(stdin)= //"`

echo "curl -X POST --data \"$PARAMS\" -H \"Key: $KEY\" -H \"Sign: $SIGN\" $SCHEMA://$HOST/tapi"
curl -X POST --data "$PARAMS" -H "Key: $KEY" -H "Sign: $SIGN" $SCHEMA://$HOST/tapi
echo
