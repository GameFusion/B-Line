
from flask import Flask, request, jsonify, abort
from flask_cors import CORS
import logging
from datetime import datetime
from models import init_db, PromptLog
from config import Config
import os

app = Flask(__name__)
app.config.from_object(Config)
CORS(app)  # Enable CORS for local network

# Logging
if app.config['LOG_LEVEL'] == 'development':
    logging.basicConfig(level=logging.DEBUG)
else:
    logging.basicConfig(level=logging.INFO)
log = logging.getLogger(__name__)

# Database
db_session = init_db(app.config['DATABASE_URL'])

@app.route('/api/v1/prompt-history', methods=['POST'])
def log_prompt_history():
    # Simple Bearer token auth
    auth_header = request.headers.get('Authorization')
    print(f"Auth Header: {auth_header}")
    print(f"Expected Token: Bearer {app.config['AUTH_TOKEN']}")
    if not auth_header or auth_header != f"Bearer {app.config['AUTH_TOKEN']}":
        abort(401, description="Unauthorized")

    data = request.get_json()
    if not data:
        abort(400, description="No JSON data provided")

    # Validate required fields
    required_fields = ['type', 'prompt', 'response']
    for field in required_fields:
        if field not in data:
            abort(400, description=f"Missing required field: {field}")

    # Create log entry
    log_entry = PromptLog(
        type=data.get('type'),
        timestamp=datetime.utcnow(),
        user=data.get('user', 'unknown'),
        version=data.get('version', '1.0'),
        llm=data.get('llm', 'unknown'),
        prompt=data.get('prompt'),
        response=data.get('response'),
        tokens=data.get('tokens', 0),
        cost=data.get('cost', 0.0),
        context=data.get('context', {})
    )

    db_session.add(log_entry)
    db_session.commit()

    log.info(f"Logged prompt: type={data['type']}, tokens={data.get('tokens', 0)}")

    return jsonify({"status": "success", "id": str(log_entry.id)}), 201

@app.errorhandler(400)
def bad_request(error):
    return jsonify({"error": error.description}), 400

@app.errorhandler(401)
def unauthorized(error):
    return jsonify({"error": error.description}), 401

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=50000, debug=True)
