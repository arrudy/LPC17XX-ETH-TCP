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
#### Command line

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