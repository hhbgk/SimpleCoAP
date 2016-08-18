#! /usr/bin/env bash


#Start-----------
ABI_TARGET=$1
SUPPORT_ARCHS="armv5 armv7 arm64"

echo_usage() {
    echo "Please enter the following commands:"
    echo "  compile-coap-dtls.sh armv5|armv7a|arm64"
    echo "  compile-coap-dtls.sh all"
    exit 1
}

case "$ABI_TARGET" in
    "")
		echo "default build armv7a"
        sh do-compile-coap-dtls.sh armv7a
    ;;
    armv5|armv7a|arm64)
		echo "build $ABI_TARGET"
		sh do-compile-coap-dtls.sh $ABI_TARGET
		echo "return $?"
    ;;
	all)
		for ARCH in $SUPPORT_ARCHS
        do
			sh do-compile-coap-dtls.sh $ARCH
        done
	;;
    *)
        echo_usage
        exit 1
    ;;
esac
