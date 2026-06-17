#!/bin/bash -e
#----------------------------------------------------------
#  Script: launch.sh
#  Launch shoreside, alpha, and bravo communities
#----------------------------------------------------------

TIME_WARP=1

for ARGI; do
    if [ "${ARGI}" = "--help" ] || [ "${ARGI}" = "-h" ]; then
        echo "launch.sh [time_warp]"
        echo "Example: ./launch.sh 10"
        exit 0
    elif [ "${ARGI//[^0-9]/}" = "$ARGI" ] && [ "$TIME_WARP" = 1 ]; then
        TIME_WARP=$ARGI
    else
        echo "launch.sh Bad arg: $ARGI"
        exit 1
    fi
done

echo "Launching shoreside, alpha, bravo with WARP: $TIME_WARP"

pAntler shoreside.moos --MOOSTimeWarp=$TIME_WARP >& /tmp/shoreside_$USER.log &
sleep 1

pAntler alpha.moos --MOOSTimeWarp=$TIME_WARP >& /tmp/alpha_$USER.log &
sleep 1

pAntler bravo.moos --MOOSTimeWarp=$TIME_WARP >& /tmp/bravo_$USER.log &
sleep 1

echo "Done launching."
echo "Logs:"
echo "  /tmp/shoreside_$USER.log"
echo "  /tmp/alpha_$USER.log"
echo "  /tmp/bravo_$USER.log"

uMAC -t shoreside.moos

echo "Killing all processes launched from this script..."
kill -- -$$