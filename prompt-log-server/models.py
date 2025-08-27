from sqlalchemy import create_engine, Column, Integer, String, Float, DateTime, JSON, Text
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import sessionmaker
from sqlalchemy.dialects.postgresql import UUID
import uuid
from datetime import datetime

Base = declarative_base()

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