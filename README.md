## ⚡ Wireless Digital Oscilloscope

## 1.Project Overview
This project consists of a circuit which is capable of plotting input volatges (AC or DC) against time and thereby displaying it on a web interface through Wi-fi as a wave. It consists of 2 probes which can be connected between any 2 points and will plot the voltage between those points against time.

The display shows RMS voltage Average Voltage and Peak-to-Peak voltage for the wave and displays what type of wave it is. The scale on both axes and the offset along the vertical can be adjusted.

The ESP-32 is powered through a USB cable from laptop making it easy to use with only the requirement of laptop and wifi.

## 2.Main Features
* Real-time display of voltage vs. time.
* Important parameters regarding the wave are displayed.
* Wave type is identified if it is triangular, sine or square wave.
* Accurate for frequencies upto 50Hz.
* Wireless meaning that ESP sends the information to the display using wifi in the form of web sockets.

## 3.Circuit Diagram and Calculation
<img width="693" height="416" alt="Screenshot 2026-02-22 171221" src="https://github.com/user-attachments/assets/fac1d4e2-6f7a-4b1d-87dc-67d8e7e77054" />

## 4.Connections 🛠️

| Component | Connects to: | Function |
| :--- | :---: | :--- |
| **Output** | GPIO 34 | ADC Channel (Sampling) |
| **Input** | Live of function generator  | Takes in the wave |
| **3.3V** | 3V3 | Power for sensors |
| **Ground** | GND | Common Ground |

## 5.Power Supply

* The ESP 32 recieves its power from the laptop through a USB.
* The input wave must be below 3.3V for usage to not damage the ESP.

## 6.Libraries used

* ESPAsyncWebServer
* AsyncTCP
* Wifi

