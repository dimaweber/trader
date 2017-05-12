#!/bin/sh
KEY="JV0VAXWU-JCEJ0GI0-XDCK9T06-4LKSSIIK-9X8BJWQN"
SECRET="f43e3a423c8a0b8ac4acd4ce316e121bc17ee06014543ea1a5c172f208cddaf9"

PARAMS=`date +method=geetInfo\&nonce=%s`
HOST="btc-e.com"
SCHEMA="https"

SIGN=`echo  -n "$PARAMS" | openssl dgst -sha512 -hmac $SECRET | sed "s/(stdin)= //"`

echo "curl -X POST --data \"$PARAMS\" -H \"Key: $KEY\" -H \"Sign: $SIGN\" $SCHEMA://$HOST/tapi"
curl -X POST --data "$PARAMS" -H "Key: $KEY" -H "Sign: $SIGN" $SCHEMA://$HOST/tapi
echo
