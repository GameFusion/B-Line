from sqlalchemy import create_engine, Column, Integer, String, Float, DateTime, JSON, Text, Boolean, ForeignKey
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import sessionmaker, relationship
from sqlalchemy.dialects.postgresql import UUID
import uuid
from datetime import datetime

Base = declarative_base()

class User(Base):
    __tablename__ = 'users'

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    username = Column(String(100), unique=True, nullable=False)
    email = Column(String(255), unique=True, nullable=False)
    hashed_password = Column(String(255), nullable=False)  # For OAuth2, store hashed token or password
    created_at = Column(DateTime, default=datetime.utcnow)
    is_active = Column(Boolean, default=True)

    # Relationship to projects
    projects = relationship("Project", back_populates="owner")

class Project(Base):
    __tablename__ = 'projects'

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    name = Column(String(255), nullable=False)
    user_id = Column(UUID(as_uuid=True), ForeignKey('users.id'), nullable=False)
    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, onupdate=datetime.utcnow)
    s3_bucket = Column(String(255))  # S3 bucket name
    s3_prefix = Column(String(255))  # S3 prefix for assets
    total_tokens = Column(Integer, default=0)
    total_cost = Column(Float, default=0.0)

    # Relationships
    owner = relationship("User", back_populates="projects")
    scenes = relationship("Scene", back_populates="project", cascade="all, delete-orphan")
    workflow_configs = relationship("WorkflowConfig", back_populates="project", cascade="all, delete-orphan")

class Scene(Base):
    __tablename__ = 'scenes'

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    project_id = Column(UUID(as_uuid=True), ForeignKey('projects.id'), nullable=False)
    name = Column(String(255), nullable=False)
    json_data = Column(JSON, nullable=False)  # Stored as JSON for scenes
    s3_key = Column(String(255))  # S3 key for scene JSON
    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, onupdate=datetime.utcnow)

    # Relationships
    project = relationship("Project", back_populates="scenes")

class WorkflowConfig(Base):
    __tablename__ = 'workflow_configs'

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    project_id = Column(UUID(as_uuid=True), ForeignKey('projects.id'), nullable=False)
    config_name = Column(String(255), nullable=False)
    config_data = Column(JSON, nullable=False)  # JSON config
    s3_key = Column(String(255))  # S3 key for config
    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, onupdate=datetime.utcnow)

    # Relationships
    project = relationship("Project", back_populates="workflow_configs")

class PromptLog(Base):
    __tablename__ = 'prompt_logs'

    id = Column(UUID(as_uuid=True), primary_key=True, default=uuid.uuid4)
    type = Column(String(50), nullable=False)  # e.g., "shots", "image_generation"
    timestamp = Column(DateTime, default=datetime.utcnow)
    user = Column(String(100))
    version = Column(String(20))
    llm = Column(String(50))  # e.g., "llama"
    prompt = Column(Text)
    response = Column(Text)
    tokens = Column(Integer)
    cost = Column(Float)
    context = Column(JSON)  # Flexible JSON for metadata (e.g., sceneId, model, etc.)
    category = Column(String(100))
    session_id = Column(String(100))
    correlation_id = Column(String(36))
    environment = Column(String(50))
    error_details = Column(JSON)
    input_size = Column(Integer)
    output_size = Column(Integer)
    latency_breakdown = Column(JSON)
    model_params = Column(JSON)
    user_id = Column(String(100))
    project_id = Column(String(100))
    date_time = Column(DateTime)

def init_db(db_url):
    engine = create_engine(db_url)
    Base.metadata.create_all(engine)
    return sessionmaker(bind=engine)()