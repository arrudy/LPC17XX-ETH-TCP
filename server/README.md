# Server

## Setup environment

To set up the server environment, follow these steps:

1. **Create a virtual environment** (optional but recommended):

   ```bash
   python -m venv venv
   ```

2. **Activate the virtual environment**:
    - On Windows:

      ```bash

      venv\Scripts\activate

      ```

    - On macOS/Linux:

      ```bash

      source venv/bin/activate

      ```

3. **Install required packages**:

    ```bash
    
    pip install -r requirements.txt
    
    ```

## Run app

To run the server application, use the following command:

```bash
python main.py
```

### Command line

```bash
send    Send message to device.
        send <client_id> <func_code> <flags> <data>
connect Connect to device.
        connect <ip> | <address>
disconnect  Disconnect device.
        disconnect <id> | "all"
list    List connected devices.
hi      Send hi message to all devices.
        hi
spam    Send spam messages to all devices.
        spam <id> | "all" | "stop"
exit    Stop server.
```

## Comunication with server

For TCP communicaton, server is always listening on port 5000

To configure other protocols for your operating system, take a look at declaration of PLATFORM_CONFIGS in main.py source file.

### Commands sent from other device

You can give server orders to do some actions.

Here is list of available commands:

- "getrandom\0" - give random naturall number between from 2 to 9

- "ta energy\0" - give current number of energy for your connected tamagotchi

- "ta casino\0" - play in casino only 2 energy for each spin

- "ta ad\0" - watch ad to get 1 energy

If your message do not match this ones, server will just ignore your message and do not take any action.
