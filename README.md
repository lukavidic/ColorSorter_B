# Color Sorter | Real-Time Tile Sorting 🟥🟩🟦

## Project Specification

The project owner has defined the following general requirements:

- Detect and sort tiles by color using a sensor-based system.  
- Develop a PC application that provides Wi-Fi communication, real-time visualization, and process control.  
- Include a local physical interface for basic start/stop control.

## Project Objective

The goal of this project is to sort colored tiles into two designated containers. The system includes:

- **PIC24FJ64GA702 MCU** on a custom PCB,  
- **Color 10 Click sensor** for color detection,  
- **SG90 servo motor** to direct each tile into its container,  
- **ESP8266-01 Wi-Fi module** for wireless link to PC,  
- **Push-button** to toggle the sorting cycle on/off,  
- **LED indicator** to show system status.  

When a tile passes under the Color 10 Click sensor, it measures the color and sends the data via I²C to the MCU.

Based on the sorting logic, the MCU drives the servo to move the tile into the correct container.

The push-button starts and stops the cycle, while the LED indicates whether the system is active.

A PC application offers a graphical interface for live monitoring and control. It communicates with the ESP8266-01 over Wi-Fi, which forwards commands to the microcontroller using UART protocol.

## Additional Information

For detailed component specifications, wiring diagrams, datasheets and code documentation please see the Wiki pages in this repository.

## Project Video Presentation

[Video](https://github.com/lukavidic/ColorSorter_B/blob/project/video/ColorSorterB.mp4)
