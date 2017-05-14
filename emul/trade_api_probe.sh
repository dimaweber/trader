#!/bin/sh

METHOD=Trade
PARAMS=`date +method=$METHOD\&nonce=%s\&amount=0.02\&rate=1588\&pair=btc_eur\&type=sell`

#KEY="2RG2YQX3-R5K2OFWG-3PSHUMC6-3M4O5A4T-LGRV3WLA"
#SECRET="110jxiqndrz5zciuu0d53isz1dioy7qr2so71nn5waui9vnt3lfawk3o5rrvas2d"

KEY='7Q4GTI2F-LD5GJSS0-1UXYT49I-HH560OBD-DD0LBTCX'
SECRET='uqu34opjgxaqvaztt6pap59yr1x0353r6pii0s9vbgkyu6up2mfzexdcd2lq7fwq'

#btce key
#KEY="2S1XKFM6-NYO3X5FH-QGHGE9IT-GLL3UEL7-QA26ESCI"
#SECRET="db1a085f7c9df4b6e235a6f0d56cb6099ba5c3fe60a799ba130d6fcb34801715"

HOST="localhost:81"
SCHEMA="http"

SIGN=`echo  -n "$PARAMS" | openssl dgst -sha512 -hmac $SECRET | sed "s/(stdin)= //"`

echo "curl -X POST --data \"$PARAMS\" -H \"Key: $KEY\" -H \"Sign: $SIGN\" $SCHEMA://$HOST/tapi"
curl -X POST --data "$PARAMS" -H "Key: $KEY" -H "Sign: $SIGN" $SCHEMA://$HOST/tapi
echo
