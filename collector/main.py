import time
import json
import argparse
from datetime import datetime
import serial
import gspread
from google.oauth2.service_account import Credentials

SCOPES = ["https://www.googleapis.com/auth/spreadsheets", "https://www.googleapis.com/auth/drive"]
BAUD_RATE = 115200


def main(sensor_config, serial_port, credential_path, spreadsheet_name):

    with open(sensor_config) as f:
        sensor_nodes = json.load(f)["sensor_nodes"]
        sensors_dict = {s["nodeID"]: s for s in sensor_nodes}
        print(f"{sensors_dict}=")

    try:
        ser = serial.Serial(serial_port, BAUD_RATE, timeout=10)
        print(f"Listening on {serial_port}...")
    except Exception as e:
        print(f"Failed to connect to Serial: {e}")
        return

    while True:
        if ser.in_waiting > 0:
            try:
                line = ser.readline().decode('utf-8').strip()
                if not line:
                    continue
                
                if not line.startswith("DATA:"):
                    print(line)
                    continue
                line = line.removeprefix("DATA:").strip()

                # Parse JSON from ESP32
                data = json.loads(line)
                print(f"received data: {data=}")
                data["timestamp"] = datetime.now().isoformat()
                print(f"added timestamp: {data=}")
                location = sensors_dict[data["nodeID"]]["location"]
                print(f"{location=}")

                creds = Credentials.from_service_account_file(credential_path, scopes=SCOPES)
                client = gspread.authorize(creds)

                sheet = client.open(spreadsheet_name).worksheet(location)

                row = [data["timestamp"], data["temperature"], data["humidity"], data["readRetries"]]
                sheet.append_row(row)

            except json.JSONDecodeError:
                print(f"Malformed serial message: {line}")
            #except Exception as ex:
            #    print(f"API/Network error: {ex}")

        time.sleep(5)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--sensor-config", required=True)
    parser.add_argument("--credential-json", required=True)
    parser.add_argument("--spreadsheet-name", required=True)
    parser.add_argument("--serial", required=True)
    args = parser.parse_args()
    main(args.sensor_config, args.serial, args.credential_json, args.spreadsheet_name)
