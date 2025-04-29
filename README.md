
# About the Project

## Project task specification provided by the project owner (general requirements):

- Sorting tiles by their color using sensor-based detection.
- A PC application that enables Wi-Fi communication and provides real-time process visualization and control.
- Physical control of the system operation.

## The goal of this project

The goal of this project is to implement the sorting of colored tiles into two colored containers.
The system consists of a servo motor and a color detection sensor, which are controlled by a PCB board
based on the PIC24FJ64GA702 microcontroller.

To ensure proper system operation, it is necessary to provide a 5V power supply to the board.
A hardware button allows the system to be started and stopped,
and an LED indicates whether the system is on or off.

For this project, a PC application has been developed which provides visualization of the tile sorting process,
as well as real-time control of the sorting process.
The application communicates over a Wi-Fi network with the ESP8266-01 Wi-Fi module,
which communicates with the microcontroller using UART.

The Color 10 Click color detection sensor communicates with the microcontroller via the I2C interface.
Its main function is to detect the color and forward it to the microcontroller.
Based on the color information and sorting process logic,
the microcontroller sends a signal to the servo motor indicating where to move the given tile.

## Additional Information

More information about the components, wiring, and their datasheets can be found on the Wiki page of this repository.

