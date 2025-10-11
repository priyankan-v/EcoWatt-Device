Private key generation: openssl ecparam -name prime256v1 -genkey -noout -out ecdsa_private.pem
Public key generation: openssl ec -in ecdsa_private.pem -pubout -out ecdsa_public.pem
Mergin binaries for wokwi: esptool --chip esp32 merge-bin -o merged-flash.bin --flash-mode dio --flash-size 4MB 0x1000 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin