from flask import Flask, request, jsonify
import pandas as pd
import os
from datetime import datetime
from middleware import require_api_key, check_crc


app = Flask(__name__)
DATA_FILE = "data.csv"

# Ensure file exists
if not os.path.exists(DATA_FILE):
    df = pd.DataFrame(columns=["timestamp", "payload"])
    df.to_csv(DATA_FILE, index=False)


def load_data():
    return pd.read_csv(DATA_FILE)

@app.before_request
def validate_api_key():
    if request.endpoint == "static":
        return
    
    validation_response = require_api_key()
    if validation_response:
        return validation_response


@app.route("/data", methods=["GET"])
def get_data():
    """Fetch all stored data"""
    df = load_data()
    return jsonify(df.to_dict(orient="records"))


# fetch the last entry
@app.route("/data/last", methods=["GET"])
def get_last_data():
    """Fetch the last entry"""
    df = load_data()
    if df.empty:
        return jsonify({"error": "No data available"}), 404
    last_entry = df.iloc[-1]
    return jsonify(last_entry.to_dict())

# fetch the last n entries that are requested
@app.route("/data/last/<int:n>", methods=["GET"])
def get_last_n_data(n):
    """Fetch the last n entries"""
    df = load_data()
    if df.empty:
        return jsonify({"error": "No data available"}), 404
    last_n_entries = df.iloc[-n:]
    return jsonify(last_n_entries.to_dict(orient="records"))

# Add new data entry
@app.route("/data", methods=["POST"])
def add_data():
    """Add a new data entry"""
    data = request.get_json()
    if not data:
        return jsonify({"error": "No data provided"}), 400
    else:
        try:
            payload = str(data["payload"])
        except KeyError as e:
            return jsonify({"error": f"Missing key: {e}"}), 400

        if not check_crc(payload):
            return jsonify({"error": "Invalid CRC"}), 400

        new_row = {
            "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "payload": payload
        }

    new_row_df = pd.DataFrame([new_row])
    new_row_df.to_csv(DATA_FILE, mode='a', header=not os.path.exists(DATA_FILE), index=False)

    return jsonify({"status": "success", "data": new_row})


if __name__ == "__main__":
    app.run(debug=True)
