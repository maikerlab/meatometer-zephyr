# Meatometer

It's grilling season! Put the sensors of your Meatometer into your lovely steak to get it done EXACTLY how you want it to be.

## User Story

I want to have a device where I can plug in multiple sensors (at least 3) to measure the core temperature of meat (like steaks) on my grill, as well as the ambient temperature inside the grill.
I want to be able to add labels to sensors, so I can easily identify where a sensor is placed (e.g. label Sensor 1 as "Ambient" (default), Sensor 2 as "Brisket", Sensor 3 as "Rumpsteak" and so on).
It should be possible to set a target temperature for each sensor.
The system should display the current operating mode, the current temperature and target temperature of all sensors. It should also show some kind of alarm/notification as soon as a target temperature of a sensor is reached.
It's not required to have a display attached, it's also fine if I could have the GUI on my mobile phone.
The system should be operated on battery and I should be able to turn it off and on using a single push button.

## Concept

### Hardware

During development phase, the dev board [nRF7002 DK](https://www.nordicsemi.com/Products/Development-hardware/nRF7002-DK) by Nordic Semiconductors is used.

Push Buttons:

- `On/Off` (Button 1 on the board, connected to GPIO P1.08): Turn device on or off
- `Measure` (Button 2 on the board, connected to GPIO P1.09): Start or stop temperature measurement

LED's:

- `Power` (LED1 on the board, GPIO P1.06): Indicates if the device is powered on
- `Measuring` (LED2 on the board, GPIO P1.07): If on, the measurement is active and temperature is continuously measured. If blinking, the target temperature was reached.

Temperature Sensors:

- Up to 3 temperature sensors (Thermocouple Type K) can be connected via an A/D converter to the SPI bus
- The idea is to use [MAX31856](https://www.analog.com/media/en/technical-documentation/data-sheets/max31856.pdf) for that, BUT a driver for this converter is not yet implemented in Zephyr
- Implementing this driver could be part of the project, but alternatively other convertes could be used, where sensor drivers exist in Zephyr (e.g. MAX31855)

### Software

The entrypoint (main function) of the app in [main.c](app/src/main.c):

- Initializes logging
- Initializes hardware
- Initializes event queue(s)
- Registers ISR for handling push button events: If any button is pressed, an event will be sent to indicate which button was pressed

Queues:

- `event_queue`:
  - Producers: Buttons (ISR), `measure_temp` -> send events to `state_machine` thread
  - Consumers: Only `state_machine` thread

Threads:

- `state_machine`: Runs a state machine to control the current state of the device by handling events received by the `event_queue`
  - Button On/Off pressed -> Toggle On/Off state
    - Off: switch to "stand-by" mode by turning off all LED's, stop any temperature measurements and put CPU to sleep
    - On: switch to "power on" state by turning on "Power" LED
  - Button Start/Stop pressed -> Toggle temperature measurement
    - Stop->Start: Continuously send `EVT_CTRL_TEMP_START` (fetch temperature measurement) to `CommandQueue`
    - Start->Stop: Just stop sending events to `CommandQueue` - `measure_temp` will then "go to sleep", because it's waiting on an empty event queue
  - Target temperature reached: Go to "Done" state
    - Turn on "Done" LED to indicate that the target temperature is reached
    - If target temperature falls again: go to previous state and continue measurement
- `measure_temp`: periodically polls temperature from sensors
  - `EVT_CTRL_TEMP_START` receive via `CommandQueue`: start sampling of temperature measurement, get value if ready and then send the value to `event_queue`
