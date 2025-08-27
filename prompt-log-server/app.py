
from flask import Flask, request, jsonify, abort
from flask_cors import CORS
from flask_httpauth import HTTPBasicAuth
from werkzeug.security import generate_password_hash, check_password_hash
import logging
from datetime import datetime
from models import init_db, User, Project, Scene, WorkflowConfig, PromptLog
from config import Config
import os
import boto3
import uuid
import json

app = Flask(__name__)
app.config.from_object(Config)
CORS(app)  # Enable CORS for local network
auth = HTTPBasicAuth()

# Logging
if app.config['LOG_LEVEL'] == 'development':
    logging.basicConfig(level=logging.DEBUG)
else:
    logging.basicConfig(level=logging.INFO)
log = logging.getLogger(__name__)

# Database
db_session = init_db(app.config['DATABASE_URL'])

# S3 Client (if AWS is used for assets)
enable_s3 = os.environ.get('ENABLE_S3', 'false').lower() == 'true'
if enable_s3:
    endpoint = os.environ.get("DO_SPACES_ENDPOINT")
    region = os.environ.get("AWS_REGION", "nyc3")
    space_name = os.environ.get("DO_SPACE_NAME")

    s3_client = boto3.client(
        "s3",
        aws_access_key_id=os.environ.get("AWS_ACCESS_KEY_ID"),
        aws_secret_access_key=os.environ.get("AWS_SECRET_ACCESS_KEY"),
        region_name=region,
        endpoint_url=endpoint   # required for DO Spaces
    )

# Simplified OAuth2-like auth (hardcoded user for MVP; replace with real OAuth2 in production)
users = {
    'andreas@gamefusion.biz': generate_password_hash('password')  # Change in production
}

@auth.verify_password
def verify_password(username, password):
    if username in users and check_password_hash(users[username], password):
        return username
    return None

# User Login (OAuth2 simplified: returns token)
@app.route('/api/v1/auth/login', methods=['POST'])
def login():
    data = request.get_json()
    username = data.get('username')
    password = data.get('password')
    if auth.verify_password(username, password):
        token = str(uuid.uuid4())  # Generate token (store in session or DB for production)
        return jsonify({'access_token': token, 'token_type': 'bearer', 'expires_in': 3600})
    return jsonify({'error': 'Invalid credentials'}), 401

# Project List
@app.route('/api/v1/projects', methods=['GET'])
@auth.login_required
def list_projects():
    username = auth.current_user()
    user = User.query.filter_by(email=username).first()
    if not user:
        abort(404, description="User not found")
    projects = Project.query.filter_by(user_id=user.id).all()
    return jsonify([{
        'id': p.id,
        'name': p.name,
        'created_at': p.created_at.isoformat(),
        's3_bucket': p.s3_bucket,
        'total_tokens': p.total_tokens,
        'total_cost': p.total_cost
    } for p in projects])

# Pull Project
@app.route('/api/v1/projects/<project_id>', methods=['GET'])
@auth.login_required
def pull_project(project_id):
    username = auth.current_user()
    user = User.query.filter_by(email=username).first()
    if not user:
        abort(404, description="User not found")
    project = Project.query.filter_by(id=project_id, user_id=user.id).first()
    if not project:
        abort(404, description="Project not found")

    # Pull scenes
    scenes = Scene.query.filter_by(project_id=project_id).all()
    scenes_data = [{'id': s.id, 'name': s.name, 'json_data': s.json_data, 's3_key': s.s3_key} for s in scenes]

    # Pull workflow configs
    configs = WorkflowConfig.query.filter_by(project_id=project_id).all()
    configs_data = [{'id': c.id, 'config_name': c.config_name, 'config_data': c.config_data, 's3_key': c.s3_key} for c in configs]

    return jsonify({
        'project': {
            'id': project.id,
            'name': project.name,
            's3_bucket': project.s3_bucket,
            's3_prefix': project.s3_prefix,
            'total_tokens': project.total_tokens,
            'total_cost': project.total_cost
        },
        'scenes': scenes_data,
        'workflow_configs': configs_data
    })

# Push Scenes/Assets
@app.route('/api/v1/projects/<project_id>/scenes', methods=['POST'])
@auth.login_required
def push_scenes(project_id):
    username = auth.current_user()
    user = User.query.filter_by(email=username).first()
    project = Project.query.filter_by(id=project_id, user_id=user.id).first()
    if not project:
        abort(404, description="Project not found")

    data = request.get_json()
    scenes = data.get('scenes', [])
    for scene_data in scenes:
        scene = Scene(
            project_id=project_id,
            name=scene_data.get('name'),
            json_data=scene_data.get('json_data'),
            s3_key=upload_to_s3(scene_data.get('json_data'), f"{project.s3_prefix}/scenes/{scene_data.get('name')}.json")
        )
        db.session.add(scene)

    db.session.commit()
    return jsonify({'message': f'Pushed {len(scenes)} scenes'}), 201

# Upload to S3 (helper function)
def upload_to_s3(data, key):
    if not project.s3_bucket:
        return None
    try:
        s3_client.put_object(
            Bucket=project.s3_bucket,
            Key=key,
            Body=json.dumps(data)
        )
        return key
    except Exception as e:
        log.error(f"S3 upload failed: {str(e)}")
        return None

# Publish/Save Project
@app.route('/api/v1/projects/<project_id>/publish', methods=['POST'])
@auth.login_required
def publish_project(project_id):
    username = auth.current_user()
    user = User.query.filter_by(email=username).first()
    project = Project.query.filter_by(id=project_id, user_id=user.id).first()
    if not project:
        abort(404, description="Project not found")

    # Save project (e.g., update total_tokens, total_cost)
    project.updated_at = datetime.utcnow()
    db.session.commit()

    return jsonify({'message': 'Project published successfully'}), 200

# AI Remote Inference (MVP: Text)
@app.route('/api/v1/ai/inference/text', methods=['POST'])
@auth.login_required
def ai_text_inference():
    data = request.get_json()
    prompt = data.get('prompt')
    if not prompt:
        abort(400, description="Missing prompt")

    # MVP: System call to local Llama or remote API (e.g., OpenAI)
    import subprocess
    try:
        # Example: Call local Llama script
        result = subprocess.run(['python', 'ai_scripts/text_inference.py', prompt], capture_output=True, text=True)
        response = result.stdout if result.returncode == 0 else {"error": result.stderr}
        return jsonify(response), 200
    except Exception as e:
        log.error(f"AI inference failed: {str(e)}")
        return jsonify({'error': 'Inference failed'}), 500

# AI Diffusion for Images (MVP)
@app.route('/api/v1/ai/diffusion/image', methods=['POST'])
@auth.login_required
def ai_image_diffusion():
    data = request.get_json()
    prompt = data.get('prompt')
    if not prompt:
        abort(400, description="Missing prompt")

    try:
        # MVP: System call to Stable Diffusion script
        result = subprocess.run(['python', 'ai_scripts/image_diffusion.py', prompt], capture_output=True, text=True)
        if result.returncode == 0:
            image_path = result.stdout.strip()
            return jsonify({'image_path': image_path}), 200
        else:
            return jsonify({'error': result.stderr}), 500
    except Exception as e:
        log.error(f"Image diffusion failed: {str(e)}")
        return jsonify({'error': 'Diffusion failed'}), 500

# AI Movie Generation (MVP)
@app.route('/api/v1/ai/movie', methods=['POST'])
@auth.login_required
def ai_movie_generation():
    data = request.get_json()
    prompt = data.get('prompt')
    if not prompt:
        abort(400, description="Missing prompt")

    try:
        # MVP: System call to text-to-video script
        result = subprocess.run(['python', 'ai_scripts/movie_generation.py', prompt], capture_output=True, text=True)
        if result.returncode == 0:
            video_path = result.stdout.strip()
            return jsonify({'video_path': video_path}), 200
        else:
            return jsonify({'error': result.stderr}), 500
    except Exception as e:
        log.error(f"Movie generation failed: {str(e)}")
        return jsonify({'error': 'Movie generation failed'}), 500

 # Workflow Configuration Pull
@app.route('/api/v1/projects/<project_id>/workflow-configs', methods=['GET'])
@auth.login_required
def pull_workflow_configs(project_id):
    username = auth.current_user()
    user = User.query.filter_by(email=username).first()
    project = Project.query.filter_by(id=project_id, user_id=user.id).first()
    if not project:
        abort(404, description="Project not found")

    configs = WorkflowConfig.query.filter_by(project_id=project_id).all()
    return jsonify([{
        'id': c.id,
        'config_name': c.config_name,
        'config_data': c.config_data,
        's3_key': c.s3_key
    } for c in configs])

# Workflow Configuration Publish
@app.route('/api/v1/projects/<project_id>/workflow-configs', methods=['POST'])
@auth.login_required
def publish_workflow_config(project_id):
    username = auth.current_user()
    user = User.query.filter_by(email=username).first()
    project = Project.query.filter_by(id=project_id, user_id=user.id).first()
    if not project:
        abort(404, description="Project not found")

    data = request.get_json()
    config_name = data.get('config_name')
    config_data = data.get('config_data')
    s3_key = upload_to_s3(config_data, f"{project.s3_prefix}/configs/{config_name}.json")

    config = WorkflowConfig(
        project_id=project_id,
        config_name=config_name,
        config_data=config_data,
        s3_key=s3_key
    )
    db.session.add(config)
    db.session.commit()

    return jsonify({'message': 'Workflow config published'}), 201

# Compute Token and Cost per Project/Session/User
@app.route('/api/v1/projects/<project_id>/stats', methods=['GET'])
@auth.login_required
def project_stats(project_id):
    username = auth.current_user()
    user = User.query.filter_by(email=username).first()
    project = Project.query.filter_by(id=project_id, user_id=user.id).first()
    if not project:
        abort(404, description="Project not found")

    # Compute total tokens and cost for project
    logs = PromptLog.query.filter_by(project_id=project_id).all()
    total_tokens = sum(log.tokens for log in logs)
    total_cost = sum(log.cost for log in logs)

    # Update project totals (optional, for caching)
    project.total_tokens = total_tokens
    project.total_cost = total_cost
    db.session.commit()

    return jsonify({
        'total_tokens': total_tokens,
        'total_cost': total_cost,
        'log_count': len(logs)
    })

# Helper: Upload to S3
def upload_to_s3(data, key):
    if not project.s3_bucket:
        return None
    try:
        s3_client.put_object(
            Bucket=project.s3_bucket,
            Key=key,
            Body=json.dumps(data)
        )
        return key
    except Exception as e:
        log.error(f"S3 upload failed: {str(e)}")
        return None

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
            type=data.get('type', 'unknown'),
            timestamp=datetime.utcnow(),
            category=data.get('category', 'unknown'),
            date_time=data.get('dateTime', None),
            user_id=data.get('userId', 'unknown'),
            project_id=data.get('projectId', 'unknown'),
            session_id=data.get('sessionId', 'unknown'),
            correlation_id=data.get('correlationId', '00000000-0000-0000-0000-000000000000'),
            environment=data.get('environment', 'unknown'),
            error_details=data.get('errorDetails', {}),
            input_size=data.get('inputSize', 0),
            output_size=data.get('outputSize', 0),
            latency_breakdown=data.get('latencyBreakdown', {}),
            model_params=data.get('modelParams', {}),
            user=data.get('user', 'unknown'),
            version=data.get('version', '1.0'),
            llm=data.get('llm', 'llama'),
            prompt=data.get('prompt', ''),
            response=data.get('response', ''),
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
