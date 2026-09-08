SN=$(date +%s%3N); \
printf '{"cmd":0,"pv":0,"sn":"%s","msg":{}}\r\n' "$SN" \
    | nc -w 2 192.168.4.1 5555
    
    


IP=10.10.10.242; \
FW=.pio/build/generic-ln882h/firmware.uf2; \
SIZE=$(stat -c%s "$FW"); \
echo "UPLOAD de $FW [$SIZE bytes] para $IP"; \
curl -v --fail-with-body -F "firmware=@$FW;type=application/octet-stream" "http://$IP/api/ota?tamanho=$SIZE"



