EESchema Schematic File Version 4
LIBS:power
LIBS:device
LIBS:Connector_Generic
EELAYER 29 0
EELAYER END
$Descr A4 11693 8268
Sheet 1 1
Title "AI-Powered Railway Multi-Connectivity Gateway V1"
Date "2026-09-01"
Rev "V1.0"
Comp "Student Research Prototype"
Comment1 "Raspberry Pi 5 + external 3-modem interface + GNSS"
Comment2 "5 V LAB POWER ONLY"
Comment3 "Do not connect directly to railway battery/traction power"
Comment4 "AI Railway Multi-Connectivity Project"
$EndDescr
Text Notes 800 700 0    120  ~ 24
AI-POWERED RAILWAY MULTI-CONNECTIVITY GATEWAY V1
Text Notes 800 900 0    70   ~ 14
Prototype interface board: Raspberry Pi 5 + external modem hub + GNSS + protected 5V lab input

$Comp
L Connector_Generic:Conn_02x20_Odd_Even J1
U 1 1 1
P 8000 2700
F 0 "J1" H 8050 3817 50 0000 C CNN
F 1 "RASPBERRY_PI_5_40PIN" H 8050 3726 50 0000 C CNN
F 2 "Connector_PinHeader_2x20_P2.54mm_Vertical" H 8000 2700 50 0001 C CNN
	1    8000 2700
	1 0 0 -1
$EndComp
$Comp
L Connector_Generic:Conn_01x04 J2
U 1 1 2
P 2200 1900
F 0 "J2" H 2118 2217 50 0000 C CNN
F 1 "MODEM_1_CTRL" H 2118 2126 50 0000 C CNN
F 2 "Connector_PinHeader_1x04_P2.54mm_Vertical" H 2200 1900 50 0001 C CNN
	1    2200 1900
	-1 0 0 -1
$EndComp
$Comp
L Connector_Generic:Conn_01x04 J3
U 1 1 3
P 2200 2900
F 0 "J3" H 2118 3217 50 0000 C CNN
F 1 "MODEM_2_CTRL" H 2118 3126 50 0000 C CNN
F 2 "Connector_PinHeader_1x04_P2.54mm_Vertical" H 2200 2900 50 0001 C CNN
	1    2200 2900
	-1 0 0 -1
$EndComp
$Comp
L Connector_Generic:Conn_01x04 J4
U 1 1 4
P 2200 3900
F 0 "J4" H 2118 4217 50 0000 C CNN
F 1 "MODEM_3_CTRL" H 2118 4126 50 0000 C CNN
F 2 "Connector_PinHeader_1x04_P2.54mm_Vertical" H 2200 3900 50 0001 C CNN
	1    2200 3900
	-1 0 0 -1
$EndComp
$Comp
L Connector_Generic:Conn_01x04 J5
U 1 1 5
P 2200 5000
F 0 "J5" H 2118 5317 50 0000 C CNN
F 1 "GNSS_UART" H 2118 5226 50 0000 C CNN
F 2 "Connector_PinHeader_1x04_P2.54mm_Vertical" H 2200 5000 50 0001 C CNN
	1    2200 5000
	-1 0 0 -1
$EndComp
$Comp
L Connector_Generic:Conn_01x02 J6
U 1 1 6
P 2200 6100
F 0 "J6" H 2118 6317 50 0000 C CNN
F 1 "5V_LAB_IN" H 2118 6226 50 0000 C CNN
F 2 "Connector_PinHeader_1x02_P2.54mm_Vertical" H 2200 6100 50 0001 C CNN
	1    2200 6100
	-1 0 0 -1
$EndComp
$Comp
L Device:Fuse F1
U 1 1 7
P 3300 6100
F 0 "F1" V 3103 6100 50 0000 C CNN
F 1 "1A" V 3194 6100 50 0000 C CNN
F 2 "Fuse:Fuse_Blade_Keystone_3568" V 3230 6100 50 0001 C CNN
	1    3300 6100
	0 1 1 0
$EndComp
$Comp
L Device:LED D1
U 1 1 8
P 4700 5750
F 0 "D1" H 4693 5495 50 0000 C CNN
F 1 "POWER" H 4693 5586 50 0000 C CNN
F 2 "LED_THT:LED_D5.0mm" H 4700 5750 50 0001 C CNN
	1    4700 5750
	0 -1 -1 0
$EndComp
$Comp
L Device:R R1
U 1 1 9
P 4700 5300
F 0 "R1" H 4770 5346 50 0000 L CNN
F 1 "1k" H 4770 5255 50 0000 L CNN
F 2 "Resistor_THT:R_Axial_DIN0207_L6.3mm_D2.5mm_P10.16mm_Horizontal" V 4630 5300 50 0001 C CNN
	1    4700 5300
	1 0 0 -1
$EndComp
$Comp
L Device:C C1
U 1 1 10
P 5300 5650
F 0 "C1" H 5415 5696 50 0000 L CNN
F 1 "100uF" H 5415 5605 50 0000 L CNN
F 2 "Capacitor_THT:C_Radial_D5.0mm_H5.0mm_P2.00mm" H 5338 5500 50 0001 C CNN
	1    5300 5650
	1 0 0 -1
$EndComp
$Comp
L Device:C C2
U 1 1 11
P 6100 5650
F 0 "C2" H 6215 5696 50 0000 L CNN
F 1 "100nF" H 6215 5605 50 0000 L CNN
F 2 "Capacitor_THT:C_Disc_D5.0mm_W2.5mm_P5.00mm" H 6138 5500 50 0001 C CNN
	1    6100 5650
	1 0 0 -1
Text Notes 1700 1550 0    70   ~ 14
EXTERNAL MODEM CONTROL/POWER HEADERS
Text Notes 1700 4550 0    70   ~ 14
GNSS + LAB POWER INPUT
Text Notes 7300 1600 0    70   ~ 14
RASPBERRY PI 5 CONTROL HEADER
Text Notes 4300 4850 0    70   ~ 14
5V POWER MONITOR / DECOUPLING
Text Label 2500 1800 0 50 ~ 0
MODEM1_5V
Text Label 2500 1900 0 50 ~ 0
MODEM1_GND
Text Label 2500 2000 0 50 ~ 0
MODEM1_EN
Text Label 2500 2100 0 50 ~ 0
MODEM1_STAT
Text Label 2500 2800 0 50 ~ 0
MODEM2_5V
Text Label 2500 2900 0 50 ~ 0
MODEM2_GND
Text Label 2500 3000 0 50 ~ 0
MODEM2_EN
Text Label 2500 3100 0 50 ~ 0
MODEM2_STAT
Text Label 2500 3800 0 50 ~ 0
MODEM3_5V
Text Label 2500 3900 0 50 ~ 0
MODEM3_GND
Text Label 2500 4000 0 50 ~ 0
MODEM3_EN
Text Label 2500 4100 0 50 ~ 0
MODEM3_STAT
Text Label 2500 4900 0 50 ~ 0
GNSS_5V
Text Label 2500 5000 0 50 ~ 0
GNSS_GND
Text Label 2500 5100 0 50 ~ 0
GNSS_TX
Text Label 2500 5200 0 50 ~ 0
GNSS_RX
Text Label 2500 6100 0 50 ~ 0
VIN_5V
Text Label 2500 6200 0 50 ~ 0
GND
Wire Wire Line
	2400 6100 3150 6100
Wire Wire Line
	3450 6100 4700 6100
Wire Wire Line
	4700 6100 4700 5900
Wire Wire Line
	4700 5450 4700 5600
Wire Wire Line
	4700 5150 4700 5000
Wire Wire Line
	5300 5500 5300 5000
Wire Wire Line
	6100 5500 6100 5000
Wire Wire Line
	4700 5000 5300 5000
Wire Wire Line
	5300 5000 6100 5000
Wire Wire Line
	2400 6200 4100 6200
Wire Wire Line
	4100 6200 4100 6100
Wire Wire Line
	4100 6100 4700 6100
Wire Wire Line
	5300 5800 5300 6200
Wire Wire Line
	6100 5800 6100 6200
Wire Wire Line
	4100 6200 5300 6200
Wire Wire Line
	5300 6200 6100 6200
Connection ~ 4700 6100
Connection ~ 5300 5000
Connection ~ 5300 6200
Connection ~ 4100 6200
Text Label 7600 1800 2 50 ~ 0
PI_3V3
Text Label 7600 1900 2 50 ~ 0
PI_5V
Text Label 7600 2000 2 50 ~ 0
PI_GND
Text Label 8500 1800 0 50 ~ 0
PI_UART_TX
Text Label 8500 1900 0 50 ~ 0
PI_UART_RX
Text Label 8500 2000 0 50 ~ 0
PI_I2C_SDA
Text Label 8500 2100 0 50 ~ 0
PI_I2C_SCL
Text Label 8500 2200 0 50 ~ 0
PI_STATUS
$EndSCHEMATC
