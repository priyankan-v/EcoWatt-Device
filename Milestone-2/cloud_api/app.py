from flask import Flask, request, jsonify
import pandas as pd
import os
from datetime import datetime
from middleware import require_api_key, check_crc, validate_content_type


app = Flask(__name__)
DATA_FILE = "data.csv"

if not os.path.exists(DATA_FILE):
    df = pd.DataFrame(columns=["timestamp", "frame"])
    df.to_csv(DATA_FILE, index=False)

def load_data():
    return pd.read_csv(DATA_FILE)

@app.before_request
def validate_request():
    """Validate API key and Content-Type for all requests."""
    if request.endpoint == "static":
        return

    # Validate API key
    validation_response = require_api_key()
    if validation_response:
        return validation_response

    # Validate Content-Type
    content_type_validation = validate_content_type()
    if content_type_validation:
        return content_type_validation

@app.route("/read/all", methods=["GET"])
def get_data():
    """Fetch all stored data"""
    df = load_data()
    return jsonify(df.to_dict(orient="records"))

@app.route("/read", methods=["GET"])
def get_last_data():
    """Fetch the last entry"""
    df = load_data()
    if df.empty:
        return jsonify({"error": "No data available"}), 404
    last_entry = df.iloc[-1]
    return jsonify(last_entry.to_dict())

@app.route("/read/<int:n>", methods=["GET"])
def get_last_n_data(n):
    """Fetch the last n entries"""
    df = load_data()
    if df.empty:
        return jsonify({"error": "No data available"}), 404
    last_n_entries = df.iloc[-n:]
    return jsonify(last_n_entries.to_dict(orient="records"))

@app.route("/write", methods=["POST"])
def add_data():
    """Add a new data entry"""
    # Validate request body
    data = request.get_json()
    if not data:
        return jsonify({"error": "Empty request body"}), 400
    else:
        try:
            frame = str(data["frame"])
        except Exception as e:
            return jsonify({"error": str(e)}), 400

        if not check_crc(frame):
            return jsonify({"error": "Malformed Frame"}), 400

        new_row = {
            "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
            "frame": frame
        }

    new_row_df = pd.DataFrame([new_row])
    new_row_df.to_csv(DATA_FILE, mode='a', header=not os.path.exists(DATA_FILE), index=False)

    return jsonify({"status": "success", "data": new_row})

if __name__ == "__main__":
    app.run(debug=True, host="127.0.0.1", port=8080)
