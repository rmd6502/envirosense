EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L RF_Module:ESP32-WROOM-32D U?
U 1 1 5E0B6DFE
P 3200 3025
F 0 "U?" H 3200 4606 50  0000 C CNN
F 1 "ESP32-WROOM-32D" H 3200 4515 50  0000 C CNN
F 2 "RF_Module:ESP32-WROOM-32" H 3200 1525 50  0001 C CNN
F 3 "https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32d_esp32-wroom-32u_datasheet_en.pdf" H 2900 3075 50  0001 C CNN
	1    3200 3025
	1    0    0    -1  
$EndComp
$Comp
L Interface_USB:CP2102N-A01-GQFN28 U?
U 1 1 5E0B8B66
P 9075 2600
F 0 "U?" H 9075 1211 50  0000 C CNN
F 1 "CP2102N-A01-GQFN28" H 9075 1120 50  0000 C CNN
F 2 "Package_DFN_QFN:QFN-28-1EP_5x5mm_P0.5mm_EP3.35x3.35mm" H 9525 1400 50  0001 L CNN
F 3 "https://www.silabs.com/documents/public/data-sheets/cp2102n-datasheet.pdf" H 9125 1850 50  0001 C CNN
	1    9075 2600
	1    0    0    -1  
$EndComp
$Comp
L Connector:USB_B_Micro J?
U 1 1 5E0BA5E7
P 7375 1900
F 0 "J?" H 7432 2367 50  0000 C CNN
F 1 "USB_B_Micro" H 7432 2276 50  0000 C CNN
F 2 "" H 7525 1850 50  0001 C CNN
F 3 "~" H 7525 1850 50  0001 C CNN
	1    7375 1900
	1    0    0    -1  
$EndComp
$Comp
L Connector_Generic:Conn_01x05 J?
U 1 1 5E0BD6D4
P 4550 5125
F 0 "J?" H 4630 5167 50  0000 L CNN
F 1 "Conn_01x05" H 4630 5076 50  0000 L CNN
F 2 "" H 4550 5125 50  0001 C CNN
F 3 "~" H 4550 5125 50  0001 C CNN
	1    4550 5125
	1    0    0    -1  
$EndComp
$Comp
L Battery_Management:BQ25504 U?
U 1 1 5E0C027F
P 7250 3625
F 0 "U?" H 7894 3671 50  0000 L CNN
F 1 "BQ25504" H 7894 3580 50  0000 L CNN
F 2 "Package_DFN_QFN:QFN-16-1EP_3x3mm_P0.5mm_EP1.8x1.8mm" H 7250 3625 50  0001 C CNN
F 3 "http://www.ti.com/lit/ds/symlink/bq25504.pdf" H 6950 4425 50  0001 C CNN
	1    7250 3625
	1    0    0    -1  
$EndComp
$EndSCHEMATC
