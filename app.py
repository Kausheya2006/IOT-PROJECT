from flask import Flask, jsonify, request
import joblib
import numpy as np
import requests
from flask_cors import CORS
from pymongo import MongoClient
import datetime

app = Flask(__name__)
CORS(app)  # Enable CORS for cross-origin requests

# Load pre-trained model and scaler
model = joblib.load('training_model/water_potability_model.pkl')
scaler = joblib.load('training_model/scaler.pkl')  # Load the scaler

from dotenv import load_dotenv
import os

# Load .env variables
load_dotenv()

# ThingSpeak channel information
THINGSPEAK_CHANNEL_ID = os.getenv("THINGSPEAK_CHANNEL_ID")
THINGSPEAK_API_KEY = os.getenv("THINGSPEAK_API_KEY")
THINGSPEAK_URL = f"https://api.thingspeak.com/channels/{THINGSPEAK_CHANNEL_ID}/feeds/last.json?api_key={THINGSPEAK_API_KEY}"

# MongoDB connection setup
MONGO_URI = os.getenv("MONGO_URI")
DB_NAME = os.getenv("DB_NAME")
COLLECTION_NAME = os.getenv("COLLECTION_NAME")

client = MongoClient(MONGO_URI)
db = client[DB_NAME]
collection = db[COLLECTION_NAME]

# Function to determine action remark based on score
def get_action_remark(score):
    if score >= 85:
        return "✅ Safe - No Action Needed"
    elif score >= 70:
        return "⚠️ Monitor Quality"
    elif score >= 60:
        return "🔶 Caution - Continous Red LED"
    elif score >= 50:
        return "🟠 Treat Water - Continuous Red LED"
    else:
        return "🔴 Unsafe - Continous Beeping"

@app.route('/predict', methods=['GET'])
def predict():
    try:
        # Fetch the latest data from ThingSpeak
        response = requests.get(THINGSPEAK_URL)
        
        if response.status_code != 200:
            return jsonify({"error": "Failed to fetch data from ThingSpeak."}), 500
        
        data = response.json()

        # Extract the necessary fields
        try:
            ph = float(data["field1"])  # Assuming 'field1' corresponds to 'ph'
            turbidity = float(data["field2"])  # Assuming 'field2' corresponds to 'Turbidity'
            solids = float(data["field3"])  # Assuming 'field3' corresponds to 'Solids'
        except KeyError:
            return jsonify({"error": "Missing required fields in the ThingSpeak data."}), 400
        except ValueError:
            return jsonify({"error": "Invalid value in the ThingSpeak data."}), 400

        # Prepare the data for prediction
        data_for_prediction = np.array([[ph, solids, turbidity]])

        # Scale the data
        scaled_data = scaler.transform(data_for_prediction)

        # Get prediction and probability
        prediction = model.predict(scaled_data)[0]
        probability = model.predict_proba(scaled_data)[0][1]

        # Result
        result = "Suitable" if prediction == 1 else "Not Suitable"
        safety_score = probability * 100

        # Get action remark based on the safety score
        action_remark = get_action_remark(safety_score)

        # Save the prediction result and related data into MongoDB
        record = {
            "ph": ph,
            "turbidity": turbidity,
            "solids": solids,
            "prediction": result,
            "safety_score": safety_score,
            "action_remark": action_remark,
            "timestamp": datetime.datetime.utcnow()  # Timestamp of the data insertion
        }

        # Insert the record into MongoDB collection
        collection.insert_one(record)

        # Return the response with action remark
        response = {
            "ph": ph,
            "turbidity": turbidity,
            "solids": solids,
            "Prediction": result,
            "Safety_Score": safety_score,
            "Action_Remark": action_remark
        }

        return jsonify(response)

    except Exception as e:
        return jsonify({"error": str(e)}), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5001, debug=True)
