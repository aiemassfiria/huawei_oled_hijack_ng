#!/system/bin/busybox sh

if [ "$#" -eq 0 ]; then
    echo "text:Fix TTL:"

    TTL="$(cat /etc/fix_ttl)"

    if [ "$TTL" -eq 0 ]; then
        echo "item: <No>:TTL_NO_FIX"
        echo "item:  Yes:TTL_FIX64"
    else
        echo "item:  No:TTL_NO_FIX"
        echo "item: <Yes>:TTL_FIX64"
    fi
fi

if [ "$#" -eq 1 ]; then
    case "$1" in
        TTL_NO_FIX )
            mount -o remount,rw /system
            echo 0 > /etc/fix_ttl
            echo 0 > /etc/disable_spe
            mount -o remount,ro /system

            /etc/fix_ttl.sh

            if xtables-multi iptables -t mangle -C OUTPUT -o eth_x -j TTL --ttl-set 64; then
                echo "text:Failed"
            else
                echo "text:Success"
            fi
            ;;
        TTL_FIX64 )
            mount -o remount,rw /system
            echo 64 > /etc/fix_ttl
            echo 1 > /etc/disable_spe
            mount -o remount,ro /system

            /etc/fix_ttl.sh

            if xtables-multi iptables -t mangle -C OUTPUT -o eth_x -j TTL --ttl-set 64; then
                echo "text:Success"
            else
                echo "text:Failed"
            fi
            ;;
        * ) 
            echo "text: wrong command mode"
            exit 1;; 
    esac
fi
