#!/bin/sh
KEY="2RG2YQX3-R5K2OFWG-3PSHUMC6-3M4O5A4T-LGRV3WLA"
SECRET="110jxiqndrz5zciuu0d53isz1dioy7qr2so71nn5waui9vnt3lfawk3o5rrvas2d"
PARAMS=`date +method=getInfo\&nonce=%s`
HOST="localhost:81"

SIGN=`echo  -n "$PARAMS" | openssl dgst -sha512 -hmac $SECRET | sed "s/(stdin)= //"`

echo "curl -X POST --data \"$PARAMS\" -H \"Key: $KEY\" -H \"Sign: $SIGN\" http://$HOST/tapi"
curl -X POST --data "$PARAMS" -H "Key: $KEY" -H "Sign: $SIGN" http://$HOST/tapi
echo
