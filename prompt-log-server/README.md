Storyboard Server Log Requests, Projects and Parameters

This is an option server component for the storyboard which can enables central hosting of
prompts, projects, configurations, asset registries and for distributed workflows and pipeline services

### Simple Flask Server for Prompt Logging Backend

Flask server that serves as a backend for the `logPromptAndResult` functionality. The server will:

- Expose a POST endpoint at `/api/v1/prompt-history` to receive JSON payloads (matching your `PromptLogger` output).
- Use simple Bearer token authentication (e.g., a hardcoded or config-based token for intranet use; easy to swap for JWT or basic auth).
- Store logs in a PostgreSQL database using SQLAlchemy ORM.
- Log incoming requests to console (and optionally files) for debugging.
- Run on `0.0.0.0` (all interfaces) for local network access (e.g., accessible from `http://192.168.1.100:5000`).
- Handle errors gracefully and return HTTP status codes.

#### Prerequisites
- Python 3.8+.
- PostgreSQL installed and running (e.g., via Docker: `docker run -p 5432:5432 -e POSTGRES_PASSWORD=password postgres`).
- Install dependencies:
  ```bash
  pip install flask sqlalchemy psycopg2-binary flask-cors python-dotenv
  ```
  - `flask`: Web framework.
  - `sqlalchemy`: ORM for PostgreSQL.
  - `psycopg2-binary`: PostgreSQL adapter.
  - `flask-cors`: Enable CORS for local network requests.
  - `python-dotenv`: Load config from `.env`.

#### Project Structure
```
prompt-log-server/
├── app.py               # Main Flask app
├── models.py            # Database models
├── config.py            # Configuration
├── requirements.txt     # Dependencies
└── .env                 # Environment variables (git ignore)
```

#### Step 1: Create `requirements.txt`
```
flask
sqlalchemy
psycopg2-binary
flask-cors
python-dotenv
```

#### Step 2: Create `.env` (Environment Variables)
```
DATABASE_URL=postgresql://postgres:password@localhost/prompt_logs
AUTH_TOKEN=your-secret-token-here  # Change this for your intranet
FLASK_ENV=development
```

- **DATABASE_URL**: Connection string (user:pass@host/dbname).
- **AUTH_TOKEN**: Bearer token for auth (change for security).


#### Step 3: Run the Server
1. **Set Up Database**:
   - Create the database:
     ```sql
     CREATE DATABASE prompt_logs;
     ```
   - Run the server:
     ```bash
     python app.py
     ```
   - Access at `http://localhost:5000` or `http://<your-ip>:5000` (e.g., `http://192.168.1.100:5000`).

2. **Test the Endpoint**:
   - Use curl (from client or Postman):
     ```bash
     curl -X POST http://localhost:5000/api/v1/prompt-history \
          -H "Content-Type: application/json" \
          -H "Authorization: Bearer your-secret-token-here" \
          -d '{
                "type": "shots",
                "prompt": "test prompt",
                "response": "test result",
                "tokens": 100,
                "cost": 0.05,
                "context": {"sceneId": "SCENE_001"}
              }'
     ```
   - Expected response: `{"status":"success","id":"uuid-here"}` (201 Created).
   - Check database:
     ```sql
     SELECT * FROM prompt_logs;
     ```
   - Logs appear in console: "Logged prompt: type=shots, tokens=100".

#### Step 4: Integrate with Client (`PromptLogger`)
Update `PromptLogger.cpp` to use the server URL from config (e.g., from `.env` or app settings). The endpoint is already `/api/v1/prompt-history`, so no changes needed if `serverUrl` is set correctly.

- In your app, set the server:
  ```cpp
  logger->setServerConfig("http://localhost:5000", "your-secret-token-here");
  ```

#### Step 5: Deploy for Intranet
- Run on a machine accessible via LAN (e.g., `0.0.0.0:5000`).
- For production, use Gunicorn:
  ```bash
  pip install gunicorn
  gunicorn -w 4 -b 0.0.0.0:5000 app:app
  ```
- Secure with HTTPS (e.g., via Nginx reverse proxy) for intranet.

#### Step 9: Commit Changes
- Stage files:
  ```bash
  git add app.py models.py config.py requirements.txt .env
  ```
- Commit:
  ```bash
  git commit -m "Implement simple Flask server for prompt logging with PostgreSQL backend"
  ```
- Push:
  ```bash
  git push origin main
  ```

### Notes
- **Auth**: Simple Bearer token for intranet; upgrade to JWT for production.
- **DB Schema**: `PromptLog` table auto-creates with `init_db`. Add indexes for `type` and `timestamp` for queries.
- **Error Handling**: Server returns 400/401; client logs errors (as in your `PromptLogger`).
- **Scalability**: For high volume, add queuing (e.g., Celery) and DB pooling.
- **Testing**: Use Postman to POST payloads; query DB with `psql`.

If you need Docker setup or extensions (e.g., GET endpoint for logs), let me know!