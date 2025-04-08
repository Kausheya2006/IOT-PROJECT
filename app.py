from flask import Flask, jsonify
import requests
import numpy as np
import pandas as pd
import joblib
from flask_cors import CORS  # Add this

app = Flask(__name__)
CORS(app)  # Enable CORS for all routes

# Load pre-trained model
model = joblib.load('model/water_quality_model.pkl')

# ThingSpeak Channel & API Key
THINGSPEAK_CHANNEL_ID = "2895356"  # Replace with your channel ID
THINGSPEAK_READ_API_KEY = "6ERGDR2M6LSC4IMF"  # Replace with your Read API key
THINGSPEAK_URL = f"https://api.thingspeak.com/channels/{THINGSPEAK_CHANNEL_ID}/feeds.json?api_key={THINGSPEAK_READ_API_KEY}&results=1"

# Default values for missing fields
DEFAULT_VALUES = {
    "ph": 7.2,
    "Hardness": 150,
    "Solids": 500,
    "Chloramines": 4.0,
    "Sulfate": 250,
    "Conductivity": 500,
    "Organic_carbon": 5.0,
    "Trihalomethanes": 0.08,
    "Turbidity": 1.0
}

# Function to calculate water safety score
def water_safety_score(probabilities):
    return probabilities * 100

# Function to determine action remark based on score
def get_action_remark(score):
    if score >= 85:
        return "✅ Safe - No Action Needed"
    elif score >= 70:
        return "⚠️ Monitor Quality - Slow Buzzer"
    elif score >= 60:
        return "🔶 Caution - Medium Buzzer"
    elif score >= 50:
        return "🟠 Treat Water - Fast Buzzer"
    else:
        return "🔴 Unsafe - Continuous Buzzer"
    
@app.route('/predict', methods=['GET'])
def predict():
    try:
        # Fetch latest data from ThingSpeak
        response = requests.get(THINGSPEAK_URL)

        if response.status_code != 200:
            return jsonify({"error": f"ThingSpeak API Error: {response.status_code}"}), 500

        data = response.json()

        if not isinstance(data, dict) or "feeds" not in data:
            return jsonify({"error": "Invalid response from ThingSpeak"}), 500

        feeds = data["feeds"]
        if not feeds:
            return jsonify({"error": "No data found in ThingSpeak"}), 500

        latest_entry = feeds[-1]  # Get the most recent entry

        # Map ThingSpeak fields to model inputs
        water_data = {
            "ph": float(latest_entry.get("field1", DEFAULT_VALUES["ph"])),
            "Turbidity": float(latest_entry.get("field2", DEFAULT_VALUES["Turbidity"])),
            "Solids": float(latest_entry.get("field3", DEFAULT_VALUES["Solids"])),
            "Conductivity": float(latest_entry.get("field4", DEFAULT_VALUES["Conductivity"])),
        }

        # Add missing fields with default values
        for key in ["Hardness", "Chloramines", "Sulfate", "Organic_carbon", "Trihalomethanes"]:
            water_data[key] = DEFAULT_VALUES[key]

        # Convert to DataFrame
        df = pd.DataFrame([water_data])

        # Ensure correct feature order
        df = df[model.feature_names_in_]

        # Predict probability of potability
        predicted_probabilities = model.predict_proba(df)[:, 1]

        # Calculate safety score
        safety_scores = water_safety_score(predicted_probabilities)

        # Get action remarks
        remarks = [get_action_remark(score) for score in safety_scores]

        # Create response
        # Convert NumPy float32 to Python float before returning
        response = {
            "Water_Safety_Score": float(safety_scores[0]),  # Convert float32 to float
            "Action_Remark": remarks[0]  # String values are fine
        }


        return jsonify(response)

    except Exception as e:
        return jsonify({"error": str(e)}), 500
    
@app.route('/data', methods=['GET'])
def get_raw_data():
    try:
        response = requests.get(THINGSPEAK_URL)

        if response.status_code != 200:
            return jsonify({"error": f"ThingSpeak API Error: {response.status_code}"}), 500

        data = response.json()

        if not isinstance(data, dict) or "feeds" not in data:
            return jsonify({"error": "Invalid response from ThingSpeak"}), 500

        feeds = data["feeds"]
        if not feeds:
            return jsonify({"error": "No data found in ThingSpeak"}), 500

        latest_entry = feeds[-1]

        # Map data and include missing values
        water_data = {
            "ph": float(latest_entry.get("field1", DEFAULT_VALUES["ph"])),
            "Turbidity": float(latest_entry.get("field2", DEFAULT_VALUES["Turbidity"])),
            "Solids": float(latest_entry.get("field3", DEFAULT_VALUES["Solids"])),
            "Conductivity": float(latest_entry.get("field4", DEFAULT_VALUES["Conductivity"])),
        }

        for key in ["Hardness", "Chloramines", "Sulfate", "Organic_carbon", "Trihalomethanes"]:
            water_data[key] = DEFAULT_VALUES[key]

        return jsonify(water_data)

    except Exception as e:
        return jsonify({"error": str(e)}), 500


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5001, debug=True)


# from flask import Flask, request, jsonify
# import numpy as np
# import pandas as pd
# import joblib

# app = Flask(__name__)

# # Load pre-trained model
# model = joblib.load('model/water_quality_model.pkl')

# # Default values for missing fields
# DEFAULT_VALUES = {
#     "ph": 7.2,
#     "Hardness": 150,
#     "Solids": 500,
#     "Chloramines": 4.0,
#     "Sulfate": 250,
#     "Conductivity": 500,
#     "Organic_carbon": 5.0,
#     "Trihalomethanes": 0.08,
#     "Turbidity": 1.0
# }

# # Function to calculate water safety score
# def water_safety_score(probabilities):
#     return probabilities * 100

# # Function to determine action remark based on score
# def get_action_remark(score):
#     if score >= 85:
#         return "✅ Safe - No Action Needed"
#     elif score >= 70:
#         return "⚠️ Monitor Quality - Slow Buzzer"
#     elif score >= 60:
#         return "🔶 Caution - Medium Buzzer"
#     elif score >= 50:
#         return "🟠 Treat Water - Fast Buzzer"
#     else:
#         return "🔴 Unsafe - Continuous Buzzer"

# @app.route('/predict', methods=['POST'])
# def predict():
#     try:
#         # Get JSON data from request
#         data = request.json

#         # Fill missing values with defaults
#         for key, default in DEFAULT_VALUES.items():
#             if key not in data or data[key] is None:
#                 data[key] = default

#         # Convert to DataFrame
#         df = pd.DataFrame([data])

#         # Ensure correct feature order
#         df = df[model.feature_names_in_]

#         # Predict probability of potability
#         predicted_probabilities = model.predict_proba(df)[:, 1]

#         # Calculate safety score
#         safety_scores = water_safety_score(predicted_probabilities)

#         # Get action remarks
#         remarks = [get_action_remark(score) for score in safety_scores]

#         # Create response
#         response = {
#             "Potability_Probability": predicted_probabilities[0],
#             "Water_Safety_Score": safety_scores[0],
#             "Action_Remark": remarks[0]
#         }

#         return jsonify({"score": float(safety_scores)})  # ✅ Convert float32 to Python float


#     except Exception as e:
#         return jsonify({"error": str(e)})

# if __name__ == '__main__':
#     app.run(debug=True)
