Private key generation: openssl ecparam -name prime256v1 -genkey -noout -out ecdsa_private.pem
Public key generation: openssl ec -in ecdsa_private.pem -pubout -out ecdsa_public.pem

Mergin binaries for wokwi: esptool --chip esp32 merge-bin -o merged-flash.bin --flash-mode dio --flash-size 4MB 0x1000 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin

Arduino framework default partions: https://github.com/espressif/arduino-esp32/blob/master/tools/partitions/

Default.csv: 
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x5000,
otadata,  data, ota,     0xe000,  0x2000,
app0,     app,  ota_0,   0x10000, 0x140000,
app1,     app,  ota_1,   0x150000,0x140000,
spiffs,   data, spiffs,  0x290000,0x160000,
coredump, data, coredump,0x3F0000,0x10000,