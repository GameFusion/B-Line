import os
from dotenv import load_dotenv

load_dotenv()

class Config:
    SECRET_KEY = os.environ.get('SECRET_KEY') or 'dev-secret-key'
    DATABASE_URL = os.environ.get('DATABASE_URL') or 'postgresql://postgres:password@localhost/prompt_logs'
    AUTH_TOKEN = os.environ.get('AUTH_TOKEN') or 'dev-token'
    LOG_LEVEL = os.environ.get('FLASK_ENV', 'development')