#!/bin/sh
KEY="3WTYG4ZB-I32T5T2V-S7UOKP61-9H8AMAAZ-R5H0BIVL"
SECRET="rarjtfm024i4axtfxqufhwjiqc2d9zle0m0yf6tbvzyzb87a2rnpvy3bvawlcrlo"

METHOD=ActiveOrders
PARAMS=`date +method=$METHOD\&nonce=%s`
HOST="localhost:81"
SCHEMA="http"

SIGN=`echo  -n "$PARAMS" | openssl dgst -sha512 -hmac $SECRET | sed "s/(stdin)= //"`

echo "curl -X POST --data \"$PARAMS\" -H \"Key: $KEY\" -H \"Sign: $SIGN\" $SCHEMA://$HOST/tapi"
curl -X POST --data "$PARAMS" -H "Key: $KEY" -H "Sign: $SIGN" $SCHEMA://$HOST/tapi
echo
