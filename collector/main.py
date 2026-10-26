import json
import argparse
from datetime import datetime
import gspread
from google.oauth2.service_account import Credentials

SCOPES = ["https://www.googleapis.com/auth/spreadsheets", "https://www.googleapis.com/auth/drive"]

def main(credential_path, spreadsheet_name):
    creds = Credentials.from_service_account_file(credential_path, scopes=SCOPES)
    client = gspread.authorize(creds)


    sheet = client.open(spreadsheet_name).worksheet("Basement1")

    row = [datetime.now().isoformat(), 20.0, 70.0]
    sheet.append_row(row)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--credential-json", required=True)
    parser.add_argument("--spreadsheet-name", required=True)
    args = parser.parse_args()
    main(args.credential_json, args.spreadsheet_name)
