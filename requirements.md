I have the following requirements:

The AtomS3 should controll a stepper motor via the TMC2209. It should provide a REST api for controlling the motor and an interactive webUI for interactivly controlling the motor via the REST API.

The Rest API should have at least the following functionality
* Get/Set microstepping via M1/M2 pins
* Get/Set frequency
* Get/set direction
* Get/set mode (Started,stopped, released)
* Get/set wifi config

On boot the device shall try to connect to a configured wifi. If not successful or not configured it shall start an softAP for configuraiton. After configuration it shall reboot.
If the device is connected the device shall provide the REST API and serve the webUI.
The device shall be discoverable via mDNS.
Logging shall be done via serial