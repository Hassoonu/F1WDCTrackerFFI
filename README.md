# F1WDCTrackerFFI
Uses a custom C file as a library to call functions from a Python file to track the WDC results in Formula 1.

Context: Formula 1 is a racing series that features 20 Drivers racing around different tracks in different parts of the world. After each race, a driver gets a certain amount of points based on position, incurred penalties, and more. At the end of the year, the driver with the most points becomes the World Champion. This championship for the drivers is called the World Drivers' Championship (WDC)


The goal of my project is to code a program that communicates with an API over the network to receive data on how the WDC is going. This data will include the drivers, their names, and the points they have at the point of program execution.


After getting this data, the program will run ElectronJS to create a window, and display this data in an organized matter. This output will show the drivers in order of how they're doing in the championship, their points, and when the next race starts.


This program will start as the user startsup their device, this is mostly to add a layer of complexity so that I can learn as much as I can while working on this project.

