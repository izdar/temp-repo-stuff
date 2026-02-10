#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <status_code_decimal>"
    echo "Example: $0 5635"
    exit 1
fi

HEX=$(printf '%x' $1)
CONTENT_TYPE=${HEX:0:2}
MESSAGE_TYPE=${HEX:2:2}

echo "Status Code: $1 (decimal) = 0x$HEX (hex)"
echo "Content Type: 0x$CONTENT_TYPE"
echo "Message Type: 0x$MESSAGE_TYPE"
echo ""
echo "Common DTLS Content Types:"
echo "  16 (0x16) - Handshake"
echo "  17 (0x17) - Application Data"
echo "  14 (0x14) - Change Cipher Spec"
echo "  15 (0x15) - Alert"
